#ifndef CONFIG_UTILS_H
#define CONFIG_UTILS_H

#include <string>

std::string GetConfigPath();
std::string ReadSessionToken();
void SaveSessionToken(const std::string& token);
void ClearSessionToken();
std::string GenerateSessionToken();

#endif

