#include "ConfigUtils.h"
#include <windows.h>
#include <shlobj.h>
#include <fstream>
#include <sstream>
#include <random>
#include <iomanip>
#include <sstream>

#pragma comment(lib, "shell32.lib")

std::string GetConfigPath() {
    char appDataPath[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, appDataPath) == S_OK) {
        std::string path = std::string(appDataPath) + "\\AegisChat";
        
        // Создаем папку, если её нет
        CreateDirectoryA(path.c_str(), NULL);
        
        return path + "\\config.txt";
    }
    return "";
}

std::string ReadSessionToken() {
    std::string configPath = GetConfigPath();
    if (configPath.empty()) return "";
    
    std::ifstream file(configPath);
    if (!file.is_open()) return "";
    
    std::string line;
    while (std::getline(file, line)) {
        // Удаляем пробелы в начале и конце
        size_t pos = line.find_first_not_of(" \t");
        if (pos != std::string::npos) {
            line = line.substr(pos);
        }
        pos = line.find_last_not_of(" \t");
        if (pos != std::string::npos) {
            line = line.substr(0, pos + 1);
        }
        
        // Проверяем, начинается ли строка с "LastSessionToken ="
        if (line.find("LastSessionToken") == 0) {
            size_t eqPos = line.find('=');
            if (eqPos != std::string::npos) {
                std::string token = line.substr(eqPos + 1);
                // Удаляем пробелы вокруг токена
                pos = token.find_first_not_of(" \t");
                if (pos != std::string::npos) {
                    token = token.substr(pos);
                }
                pos = token.find_last_not_of(" \t");
                if (pos != std::string::npos) {
                    token = token.substr(0, pos + 1);
                }
                file.close();
                return token;
            }
        }
    }
    
    file.close();
    return "";
}

void SaveSessionToken(const std::string& token) {
    std::string configPath = GetConfigPath();
    if (configPath.empty()) return;
    
    std::ofstream file(configPath, std::ios::out | std::ios::trunc);
    if (file.is_open()) {
        file << "LastSessionToken = " << token << std::endl;
        file.close();
    }
}

void ClearSessionToken() {
    std::string configPath = GetConfigPath();
    if (configPath.empty()) return;
    
    std::ofstream file(configPath, std::ios::out | std::ios::trunc);
    if (file.is_open()) {
        file.close();
    }
}

std::string GenerateSessionToken() {
    // Генерируем 16-значный токен из случайных символов (буквы и цифры)
    const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    const size_t charsetSize = sizeof(charset) - 1;
    const int tokenLength = 16;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, charsetSize - 1);
    
    std::string token;
    token.reserve(tokenLength);
    for (int i = 0; i < tokenLength; ++i) {
        token += charset[dis(gen)];
    }
    
    return token;
}

