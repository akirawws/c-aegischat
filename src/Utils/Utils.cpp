#include "Utils.h"
#include <cstdio>
#include <algorithm>
#include <filesystem>
#include <cstring>
namespace fs = std::filesystem;

std::string g_cacheDir = "cache";

// Простой ключ для XOR "шифрования" сообщений.
// При желании его можно вынести в конфиг или сгенерировать динамически.
static const char* kMessageXorKey = "AegisChatSecretKey";

static void XorBuffer(char* buffer, size_t maxLen) {
    if (!buffer || maxLen == 0 || !kMessageXorKey || !kMessageXorKey[0]) return;

    size_t keyLen = std::strlen(kMessageXorKey);
    size_t len = strnlen(buffer, maxLen);

    for (size_t i = 0; i < len; ++i) {
        buffer[i] ^= kMessageXorKey[i % keyLen];
    }
}

void EncryptMessage(char* buffer, size_t maxLen) {
    XorBuffer(buffer, maxLen);
}

void DecryptMessage(char* buffer, size_t maxLen) {
    XorBuffer(buffer, maxLen);
}

std::string GetUserAvatarPath(const std::string& username) {
    if (username.empty()) return "";

    std::string safeUsername = username;
    std::replace(safeUsername.begin(), safeUsername.end(), '\\', '_');
    std::replace(safeUsername.begin(), safeUsername.end(), '/', '_');
    std::replace(safeUsername.begin(), safeUsername.end(), ':', '_');

    // Попробуем возможные расширения
    std::vector<std::string> extensions = {".jpg", ".jpeg", ".png", ".bmp", ".gif"};
    for (const auto& ext : extensions) {
        std::string path = g_cacheDir + "/" + safeUsername + ext;
        if (fs::exists(path)) {
            return path;
        }
    }
    return "";
}

std::string WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

std::string GetAvatar(const std::string& name) {
    if (name.empty()) return "[?]";
    int hash = 0;
    for (char c : name) hash += c;
    std::string avatars[] = { "[^.^]", "[o_o]", "[x_x]", "[9_9]", "[@_@]", "[*_*]" };
    return avatars[hash % 6];
}

std::string GetCurrentTimeStr() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    char buf[10];
    sprintf(buf, "%02d:%02d", st.wHour, st.wMinute);
    return std::string(buf);
}
std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

void WriteLog(const std::string& text) {
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    std::string strPath = path;
    strPath = strPath.substr(0, strPath.find_last_of("\\/")) + "\\debug_log.txt";

    FILE* f = fopen(strPath.c_str(), "a");
    if (f) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        fprintf(f, "[%02d:%02d:%02d] %s\n", st.wHour, st.wMinute, st.wSecond, text.c_str());
        fflush(f);
        fclose(f);
    }
}