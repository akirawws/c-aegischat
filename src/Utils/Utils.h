#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>
#include <windows.h>

struct Message {
    std::string text;
    bool isUser;
    std::string sender;
    std::string timeStr;
};

std::string GetAvatar(const std::string& name);
std::string GetCurrentTimeStr();
void WriteLog(const std::string& text);
std::string WideToUtf8(const std::wstring& wstr);
std::string HashPassword(const std::string& password, const std::string& salt);

#endif