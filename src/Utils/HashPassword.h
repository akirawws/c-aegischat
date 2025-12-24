#ifndef HASH_PASSWORD_H
#define HASH_PASSWORD_H

#include <string>
std::string HashPassword(const std::string& password, const std::string& salt);

#endif 