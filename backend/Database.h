#ifndef DATABASE_H
#define DATABASE_H

#include <libpq-fe.h>
#include <string>
#include <vector>

class Database {
public:
    Database();
    ~Database();

    bool Connect();
    void Disconnect();
    
    bool Execute(const std::string& query);
    
    PGresult* Query(const std::string& query);

private:
    PGconn* conn;
    std::string connInfo;
};

#endif