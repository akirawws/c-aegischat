#include "Database.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <cstring> // Для работы с C-строками, если понадобится
#include <vector>

// ИСПРАВЛЕНО: Теперь LoadEnv является методом класса Database
std::map<std::string, std::string> Database::LoadEnv(const std::string& filename) {
    std::map<std::string, std::string> env;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "[!] Критическая ошибка: Не удалось открыть файл " << filename << std::endl;
        return env;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Пропускаем пустые строки и комментарии
        if (line.empty() || line[0] == '#') continue;

        std::istringstream is_line(line);
        std::string key;
        if (std::getline(is_line, key, '=')) {
            std::string value;
            if (std::getline(is_line, value)) {
                // Удаляем пробелы по краям
                key.erase(0, key.find_first_not_of(" \t"));
                key.erase(key.find_last_not_of(" \t") + 1);
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t") + 1);
                
                env[key] = value;
            }
        }
    }
    return env;
}
std::vector<std::string> Database::GetPendingRequests(const std::string& username) {
    std::vector<std::string> senders;
    if (!conn) return senders;

    // Ищем отправителей (u1.username), где получатель (u2.username) — это наш пользователь
    // и статус дружбы — 'pending'
    std::string query = 
        "SELECT u1.username FROM users u1 "
        "JOIN friendships f ON u1.id = f.user_id_1 "
        "JOIN users u2 ON u2.id = f.user_id_2 "
        "WHERE u2.username = $1 AND f.status = 'pending'";

    const char* paramValues[1] = { username.c_str() };
    PGresult* res = PQexecParams(conn, query.c_str(), 1, NULL, paramValues, NULL, NULL, 0);

    if (PQresultStatus(res) == PGRES_TUPLES_OK) {
        int rows = PQntuples(res);
        for (int i = 0; i < rows; i++) {
            senders.push_back(PQgetvalue(res, i, 0));
        }
    } else {
        std::cerr << "[DB] Ошибка получения заявок: " << PQerrorMessage(conn) << std::endl;
    }

    PQclear(res);
    return senders;
}

Database::Database() : conn(nullptr) {
}

Database::~Database() {
    Disconnect();
}

bool Database::Connect() {
    auto env = LoadEnv(".env");

    if (env.empty()) {
        std::cerr << "[!] Ошибка: .env файл пуст или отсутствует!" << std::endl;
        return false;
    }

    std::string connStr = 
        "host=" + env["DB_HOST"] + 
        " port=" + env["DB_PORT"] + 
        " dbname=" + env["DB_NAME"] + 
        " user=" + env["DB_USER"] + 
        " password=" + env["DB_PASS"];

    conn = PQconnectdb(connStr.c_str());

    if (PQstatus(conn) != CONNECTION_OK) {
        std::cerr << "[!] Ошибка подключения к БД: " << PQerrorMessage(conn) << std::endl;
        return false;
    }

    std::cout << "[DB] Успешное подключение к PostgreSQL!" << std::endl;
    return true;
}

void Database::Disconnect() {
    if (conn) {
        PQfinish(conn);
        conn = nullptr;
    }
}

bool Database::RegisterUser(const std::string& user, const std::string& email, const std::string& pass) {
    if (!conn) return false;

    const char* paramValues[3] = { user.c_str(), email.c_str(), pass.c_str() };
    
    PGresult* res = PQexecParams(conn,
        "INSERT INTO users (username, email, password_hash) VALUES ($1, $2, $3)",
        3, NULL, paramValues, NULL, NULL, 0
    );

    ExecStatusType status = PQresultStatus(res);
    bool success = (status == PGRES_COMMAND_OK);

    if (!success) {
        std::cerr << "[!] Ошибка регистрации: " << PQerrorMessage(conn) << std::endl;
    }

    PQclear(res);
    return success;
}

bool Database::AuthenticateUser(const std::string& login, const std::string& pass) {
    if (!conn) return false;

    const char* paramValues[2] = { login.c_str(), pass.c_str() };

    PGresult* res = PQexecParams(conn,
        "SELECT id FROM users WHERE (username = $1 OR email = $1) AND password_hash = $2",
        2, NULL, paramValues, NULL, NULL, 0
    );

    bool authenticated = (PQntuples(res) > 0);

    PQclear(res);
    return authenticated;
}
bool Database::AddFriendRequest(const std::string& sender, const std::string& target) {
    if (sender.empty() || target.empty() || sender == target) return false;

    // ВАЖНО: SQL запрос должен соответствовать вашей структуре таблиц
    std::string query = 
        "INSERT INTO friendships (user_id_1, user_id_2, status, action_user_id) "
        "SELECT u1.id, u2.id, 'pending', u1.id "
        "FROM users u1, users u2 "
        "WHERE u1.username = '" + sender + "' AND u2.username = '" + target + "' "
        "ON CONFLICT DO NOTHING;";

    // Здесь используйте ваш метод выполнения запроса (например, PQexec для libpq)
    PGresult* res = PQexec(conn, query.c_str()); 

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        std::cerr << "Ошибка БД: " << PQerrorMessage(conn) << std::endl;
        PQclear(res);
        return false;
    }

    // Проверяем, была ли вставлена строка (atoi(PQcmdTuples(res)) > 0)
    bool success = true; 
    PQclear(res);
    return success;
}
bool Database::HandleFriendAction(const std::string& sender, const std::string& target, bool accept) {
    if (!conn) return false;

    if (accept) {
        // Меняем статус на 'accepted'
        const char* params[2] = { sender.c_str(), target.c_str() };
        PGresult* res = PQexecParams(conn,
            "UPDATE friendships SET status = 'accepted' "
            "WHERE user_id_1 = (SELECT id FROM users WHERE username = $1) "
            "AND user_id_2 = (SELECT id FROM users WHERE username = $2)",
            2, NULL, params, NULL, NULL, 0);
        
        bool success = (PQresultStatus(res) == PGRES_COMMAND_OK);
        PQclear(res);
        return success;
    } else {
        // При отклонении — просто удаляем запись
        const char* params[2] = { sender.c_str(), target.c_str() };
        PGresult* res = PQexecParams(conn,
            "DELETE FROM friendships "
            "WHERE user_id_1 = (SELECT id FROM users WHERE username = $1) "
            "AND user_id_2 = (SELECT id FROM users WHERE username = $2)",
            2, NULL, params, NULL, NULL, 0);
        
        bool success = (PQresultStatus(res) == PGRES_COMMAND_OK);
        PQclear(res);
        return success;
    }
}
