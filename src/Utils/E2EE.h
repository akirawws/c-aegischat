#ifndef E2EE_H
#define E2EE_H

#include <string>
#include <vector>
#include <cstdint>

// Минимальная E2EE-реализация для Windows через CNG (bcrypt):
// - ECDH P-256 для получения общего секрета
// - KDF (SHA-256) -> 32-байтный ключ
// - AES-256-GCM для шифрования/дешифрования сообщений
//
// Сервер не участвует в криптографии и видит только публичные ключи и ciphertext.

struct E2EESessionState {
    bool hasLocalKey = false;
    bool hasRemoteKey = false;
    bool hasSessionKey = false;

    std::vector<uint8_t> localPublicKeyBlob;   // 72 bytes (BCRYPT_ECCPUBLIC_BLOB)
    std::vector<uint8_t> remotePublicKeyBlob;  // 72 bytes (BCRYPT_ECCPUBLIC_BLOB)
    std::vector<uint8_t> sessionKey;           // 32 bytes
};

bool E2EEEnsureLocalKey(E2EESessionState& st);
const std::vector<uint8_t>& E2EEGetLocalPublicKeyBlob(const E2EESessionState& st);

// Derive session key when both local private and remote public exist.
bool E2EEDeriveSessionKey(E2EESessionState& st);

// Set remote public key (from peer)
bool E2EESetRemotePublicKey(E2EESessionState& st, const uint8_t* blob, size_t blobLen);

// Encrypt/decrypt (AES-GCM). Returns false on error.
bool E2EEEncrypt(const E2EESessionState& st,
                 const uint8_t* plaintext, size_t plaintextLen,
                 uint8_t* outNonce12,
                 uint8_t* outTag16,
                 uint8_t* outCiphertext, size_t outCipherMax,
                 uint16_t& outCipherLen);

bool E2EEDecrypt(const E2EESessionState& st,
                 const uint8_t* nonce12,
                 const uint8_t* tag16,
                 const uint8_t* ciphertext, size_t cipherLen,
                 uint8_t* outPlaintext, size_t outPlainMax,
                 size_t& outPlainLen);

#endif
