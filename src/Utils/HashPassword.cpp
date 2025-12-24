#include <windows.h>
#include <bcrypt.h>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>

std::string HashPassword(const std::string& password, const std::string& salt) {
    BCRYPT_ALG_HANDLE hAlg = NULL;
    BCRYPT_HASH_HANDLE hHash = NULL;
    NTSTATUS status;
    DWORD cbData = 0, cbHash = 0, cbHashObject = 0;
    
    // Открываем алгоритм SHA256
    status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, 0);
    
    // Вычисляем размер буфера для хеша
    status = BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PBYTE)&cbHash, sizeof(DWORD), &cbData, 0);
    status = BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PBYTE)&cbHashObject, sizeof(DWORD), &cbData, 0);

    std::vector<BYTE> hashObject(cbHashObject);
    std::vector<BYTE> hash(cbHash);

    status = BCryptCreateHash(hAlg, &hHash, hashObject.data(), cbHashObject, NULL, 0, 0);

    // Добавляем соль и пароль в хеш
    std::string saltedInput = password + salt; 
    status = BCryptHashData(hHash, (PBYTE)saltedInput.c_str(), (ULONG)saltedInput.length(), 0);
    
    status = BCryptFinishHash(hHash, hash.data(), cbHash, 0);

    // Превращаем в Hex-строку
    std::stringstream ss;
    for (BYTE b : hash) ss << std::hex << std::setw(2) << std::setfill('0') << (int)b;

    if (hHash) BCryptDestroyHash(hHash);
    if (hAlg) BCryptCloseAlgorithmProvider(hAlg, 0);

    return ss.str();
}