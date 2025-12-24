#ifndef DATABASE_H
#define DATABASE_H

#include <libpq-fe.h>
#include <string>
#include <map>

class Database {
public:
    Database();
    ~Database();

    bool Connect();
    void Disconnect();
    
    // Новые методы для авторизации
    bool RegisterUser(const std::string& user, const std::string& email, const std::string& pass);
    bool AuthenticateUser(const std::string& login, const std::string& pass);

private:
    PGconn* conn;
    std::map<std::string, std::string> LoadEnv(const std::string& filename);
};

#endif