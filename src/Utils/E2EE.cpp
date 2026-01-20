#include "E2EE.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <bcrypt.h>
#include <vector>
#include <cstring>

#pragma comment(lib, "bcrypt.lib")

// We keep private key handle per-session in a static map-like store by embedding it in the state via an internal pointer.
// To avoid exposing bcrypt types in header, store the handle in a hidden static per-state registry.
struct SessionHandles {
    BCRYPT_ALG_HANDLE ecdhAlg = nullptr;
    BCRYPT_KEY_HANDLE ecdhKeyPair = nullptr;
    BCRYPT_SECRET_HANDLE secret = nullptr;
    bool initialized = false;
};

static SessionHandles& GetHandles(E2EESessionState* st) {
    // One handle set per state instance.
    // NOTE: this assumes the state object is stable in memory (stored in map, not moved).
    static std::vector<std::pair<E2EESessionState*, SessionHandles>> g;
    for (auto& p : g) if (p.first == st) return p.second;
    g.push_back({st, SessionHandles{}});
    return g.back().second;
}

static void CleanupHandles(E2EESessionState* st) {
    auto& h = GetHandles(st);
    if (h.secret) { BCryptDestroySecret(h.secret); h.secret = nullptr; }
    if (h.ecdhKeyPair) { BCryptDestroyKey(h.ecdhKeyPair); h.ecdhKeyPair = nullptr; }
    if (h.ecdhAlg) { BCryptCloseAlgorithmProvider(h.ecdhAlg, 0); h.ecdhAlg = nullptr; }
    h.initialized = false;
}

static bool NTSuccess(NTSTATUS s) { return s >= 0; }

bool E2EEEnsureLocalKey(E2EESessionState& st) {
    if (st.hasLocalKey && !st.localPublicKeyBlob.empty()) return true;

    auto& h = GetHandles(&st);
    if (!h.initialized) {
        NTSTATUS s = BCryptOpenAlgorithmProvider(&h.ecdhAlg, BCRYPT_ECDH_P256_ALGORITHM, NULL, 0);
        if (!NTSuccess(s)) return false;
        h.initialized = true;
    }

    if (!h.ecdhKeyPair) {
        NTSTATUS s = BCryptGenerateKeyPair(h.ecdhAlg, &h.ecdhKeyPair, 256, 0);
        if (!NTSuccess(s)) { CleanupHandles(&st); return false; }
        s = BCryptFinalizeKeyPair(h.ecdhKeyPair, 0);
        if (!NTSuccess(s)) { CleanupHandles(&st); return false; }
    }

    // Export public key blob
    ULONG cb = 0;
    NTSTATUS s = BCryptExportKey(h.ecdhKeyPair, NULL, BCRYPT_ECCPUBLIC_BLOB, NULL, 0, &cb, 0);
    if (!NTSuccess(s) || cb == 0) return false;

    std::vector<uint8_t> blob(cb);
    s = BCryptExportKey(h.ecdhKeyPair, NULL, BCRYPT_ECCPUBLIC_BLOB, blob.data(), cb, &cb, 0);
    if (!NTSuccess(s)) return false;

    // Expect 72 bytes for P-256 public blob (header 8 + 64)
    st.localPublicKeyBlob = std::move(blob);
    st.hasLocalKey = true;
    return true;
}

const std::vector<uint8_t>& E2EEGetLocalPublicKeyBlob(const E2EESessionState& st) {
    return st.localPublicKeyBlob;
}

bool E2EESetRemotePublicKey(E2EESessionState& st, const uint8_t* blob, size_t blobLen) {
    if (!blob || blobLen == 0) return false;
    st.remotePublicKeyBlob.assign(blob, blob + blobLen);
    st.hasRemoteKey = true;
    st.hasSessionKey = false;
    st.sessionKey.clear();
    return true;
}

bool E2EEDeriveSessionKey(E2EESessionState& st) {
    if (!st.hasLocalKey || !st.hasRemoteKey) return false;
    if (st.hasSessionKey && st.sessionKey.size() == 32) return true;

    auto& h = GetHandles(&st);
    if (!h.initialized || !h.ecdhAlg || !h.ecdhKeyPair) return false;

    // Import peer public key
    BCRYPT_KEY_HANDLE peerPub = nullptr;
    NTSTATUS s = BCryptImportKeyPair(h.ecdhAlg, NULL, BCRYPT_ECCPUBLIC_BLOB,
                                    &peerPub,
                                    (PUCHAR)st.remotePublicKeyBlob.data(),
                                    (ULONG)st.remotePublicKeyBlob.size(),
                                    0);
    if (!NTSuccess(s)) return false;

    if (h.secret) { BCryptDestroySecret(h.secret); h.secret = nullptr; }
    s = BCryptSecretAgreement(h.ecdhKeyPair, peerPub, &h.secret, 0);
    BCryptDestroyKey(peerPub);
    peerPub = nullptr;
    if (!NTSuccess(s)) return false;

    // Derive 32-byte key via KDF hash (SHA-256)
    BCRYPT_BUFFER kdfParamsBuf[1];
    BCRYPT_BUFFER_DESC kdfParamsDesc;
    kdfParamsBuf[0].BufferType = KDF_HASH_ALGORITHM;
    kdfParamsBuf[0].pvBuffer = (PVOID)BCRYPT_SHA256_ALGORITHM;
    kdfParamsBuf[0].cbBuffer = (ULONG)wcslen(BCRYPT_SHA256_ALGORITHM) * sizeof(WCHAR);

    kdfParamsDesc.ulVersion = BCRYPTBUFFER_VERSION;
    kdfParamsDesc.cBuffers = 1;
    kdfParamsDesc.pBuffers = kdfParamsBuf;

    std::vector<uint8_t> key(32);
    ULONG out = 0;
    s = BCryptDeriveKey(h.secret, BCRYPT_KDF_HASH, &kdfParamsDesc, key.data(), (ULONG)key.size(), &out, 0);
    if (!NTSuccess(s) || out != 32) return false;

    st.sessionKey = std::move(key);
    st.hasSessionKey = true;
    return true;
}

static bool AESGcmCrypt(bool encrypt,
                        const uint8_t* key32,
                        const uint8_t* nonce12,
                        uint8_t* tag16,
                        const uint8_t* in, size_t inLen,
                        uint8_t* out, size_t outMax,
                        size_t& outLen) {
    outLen = 0;
    if (!key32 || !nonce12 || !tag16 || !in || !out) return false;
    if (inLen > outMax) return false;

    BCRYPT_ALG_HANDLE aesAlg = nullptr;
    BCRYPT_KEY_HANDLE aesKey = nullptr;

    NTSTATUS s = BCryptOpenAlgorithmProvider(&aesAlg, BCRYPT_AES_ALGORITHM, NULL, 0);
    if (!NTSuccess(s)) return false;

    // GCM mode
    s = BCryptSetProperty(aesAlg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_GCM,
                          (ULONG)wcslen(BCRYPT_CHAIN_MODE_GCM) * sizeof(WCHAR), 0);
    if (!NTSuccess(s)) { BCryptCloseAlgorithmProvider(aesAlg, 0); return false; }

    ULONG keyObjLen = 0, cbRes = 0;
    s = BCryptGetProperty(aesAlg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&keyObjLen, sizeof(keyObjLen), &cbRes, 0);
    if (!NTSuccess(s) || keyObjLen == 0) { BCryptCloseAlgorithmProvider(aesAlg, 0); return false; }

    std::vector<uint8_t> keyObj(keyObjLen);
    s = BCryptGenerateSymmetricKey(aesAlg, &aesKey, keyObj.data(), keyObjLen, (PUCHAR)key32, 32, 0);
    if (!NTSuccess(s)) { BCryptCloseAlgorithmProvider(aesAlg, 0); return false; }

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
    BCRYPT_INIT_AUTH_MODE_INFO(info);
    info.pbNonce = (PUCHAR)nonce12;
    info.cbNonce = 12;
    info.pbTag = (PUCHAR)tag16;
    info.cbTag = 16;
    info.cbData = 0;

    ULONG cbOut = 0;
    if (encrypt) {
        s = BCryptEncrypt(aesKey, (PUCHAR)in, (ULONG)inLen, &info, NULL, 0, out, (ULONG)outMax, &cbOut, 0);
    } else {
        s = BCryptDecrypt(aesKey, (PUCHAR)in, (ULONG)inLen, &info, NULL, 0, out, (ULONG)outMax, &cbOut, 0);
    }

    BCryptDestroyKey(aesKey);
    BCryptCloseAlgorithmProvider(aesAlg, 0);

    if (!NTSuccess(s)) return false;
    outLen = cbOut;
    return true;
}

bool E2EEEncrypt(const E2EESessionState& st,
                 const uint8_t* plaintext, size_t plaintextLen,
                 uint8_t* outNonce12,
                 uint8_t* outTag16,
                 uint8_t* outCiphertext, size_t outCipherMax,
                 uint16_t& outCipherLen) {
    outCipherLen = 0;
    if (!st.hasSessionKey || st.sessionKey.size() != 32) return false;
    if (!plaintext || plaintextLen == 0) return false;
    if (!outNonce12 || !outTag16 || !outCiphertext) return false;

    // Random nonce
    if (!NTSuccess(BCryptGenRandom(NULL, outNonce12, 12, BCRYPT_USE_SYSTEM_PREFERRED_RNG))) return false;
    memset(outTag16, 0, 16);

    size_t cipherLen = 0;
    if (!AESGcmCrypt(true, st.sessionKey.data(), outNonce12, outTag16,
                     plaintext, plaintextLen, outCiphertext, outCipherMax, cipherLen)) {
        return false;
    }

    if (cipherLen > 0xFFFF) return false;
    outCipherLen = (uint16_t)cipherLen;
    return true;
}

bool E2EEDecrypt(const E2EESessionState& st,
                 const uint8_t* nonce12,
                 const uint8_t* tag16,
                 const uint8_t* ciphertext, size_t cipherLen,
                 uint8_t* outPlaintext, size_t outPlainMax,
                 size_t& outPlainLen) {
    outPlainLen = 0;
    if (!st.hasSessionKey || st.sessionKey.size() != 32) return false;
    if (!nonce12 || !tag16 || !ciphertext || !outPlaintext) return false;
    if (cipherLen == 0) return false;

    // Need a mutable copy of tag for BCryptDecrypt
    uint8_t tagCopy[16];
    memcpy(tagCopy, tag16, 16);

    size_t plainLen = 0;
    if (!AESGcmCrypt(false, st.sessionKey.data(), nonce12, tagCopy,
                     ciphertext, cipherLen, outPlaintext, outPlainMax, plainLen)) {
        return false;
    }
    outPlainLen = plainLen;
    return true;
}

