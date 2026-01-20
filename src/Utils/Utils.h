#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>
#include <windows.h>

struct Message {
    std::string sender;
    std::string text;
    bool isMine = false;     
    std::string time;           
    bool isUser = false;        
    std::string timeStr;        
};
struct ChatCache {
    std::vector<Message> messages;
    int oldestOffset = 0;
    bool fullyLoaded = false;
};

// Простейшее симметричное "шифрование" (XOR) содержимого сообщений.
// Шифруем перед отправкой, расшифровываем после получения.
// Важно: это защита "от любопытных глаз", а не криптостойкая безопасность.
void EncryptMessage(char* buffer, size_t maxLen);
void DecryptMessage(char* buffer, size_t maxLen);


std::string GetAvatar(const std::string& name);
std::string GetCurrentTimeStr();
void WriteLog(const std::string& text);
std::string WideToUtf8(const std::wstring& wstr);
std::string HashPassword(const std::string& password, const std::string& salt);
std::wstring Utf8ToWide(const std::string& str);
std::string GetUserAvatarPath(const std::string& username);

#endif