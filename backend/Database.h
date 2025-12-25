#ifndef DATABASE_H
#define DATABASE_H

#include <libpq-fe.h>
#include <string>
#include <map>
#include <vector> 

class Database {
public:
    Database();
    ~Database();

    bool Connect();
    void Disconnect();
    bool HandleFriendAction(const std::string& senderName, const std::string& myName, bool accept);
    bool RegisterUser(const std::string& user, const std::string& email, const std::string& pass);
    bool AuthenticateUser(const std::string& login, const std::string& pass);
    bool AddFriendRequest(const std::string& sender, const std::string& target);
    std::vector<std::string> GetPendingRequests(const std::string& username);
private:
    PGconn* conn; 
    std::map<std::string, std::string> LoadEnv(const std::string& filename);
};

#endif