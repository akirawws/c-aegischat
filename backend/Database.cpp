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
bool Database::AcceptFriendAndCreateRoom(const std::string& sender, const std::string& target) {
    if (!conn) return false;

    PQexec(conn, "BEGIN");

    const char* params[2] = { sender.c_str(), target.c_str() };
    PGresult* res1 = PQexecParams(conn,
        "UPDATE friendships SET status = 'accepted' "
        "WHERE (user_id_1 = (SELECT id FROM users WHERE username = $1) AND user_id_2 = (SELECT id FROM users WHERE username = $2)) "
        "OR (user_id_1 = (SELECT id FROM users WHERE username = $2) AND user_id_2 = (SELECT id FROM users WHERE username = $1))",
        2, NULL, params, NULL, NULL, 0);

    if (PQresultStatus(res1) != PGRES_COMMAND_OK) {
        PQexec(conn, "ROLLBACK");
        PQclear(res1);
        return false;
    }

    // 2. Создаем комнату, гарантируя, что меньший ID всегда первый (чтобы избежать дублей)
    std::string createRoomQuery = 
        "INSERT INTO dm_rooms (user_1, user_2) "
        "SELECT LEAST(u1.id, u2.id), GREATEST(u1.id, u2.id) "
        "FROM users u1, users u2 "
        "WHERE u1.username = $1 AND u2.username = $2 "
        "ON CONFLICT DO NOTHING";

    PGresult* res2 = PQexecParams(conn, createRoomQuery.c_str(), 2, NULL, params, NULL, NULL, 0);

    if (PQresultStatus(res2) != PGRES_COMMAND_OK) {
        PQexec(conn, "ROLLBACK");
        PQclear(res1); PQclear(res2);
        return false;
    }

    PQexec(conn, "COMMIT");
    PQclear(res1); PQclear(res2);
    return true;
}

std::vector<std::string> Database::GetAcceptedFriends(const std::string& username) {
    std::vector<std::string> friends;
    if (!conn) return friends;

    const char* params[1] = { username.c_str() };

    std::string query = 
        "SELECT CASE WHEN u1.username = $1 THEN u2.username ELSE u1.username END "
        "FROM friendships f "
        "JOIN users u1 ON f.user_id_1 = u1.id "
        "JOIN users u2 ON f.user_id_2 = u2.id "
        "WHERE (u1.username = $1 OR u2.username = $1) AND f.status = 'accepted'";

    PGresult* res = PQexecParams(conn, query.c_str(), 1, NULL, params, NULL, NULL, 0);

    if (PQresultStatus(res) == PGRES_TUPLES_OK) {
        for (int i = 0; i < PQntuples(res); i++) {
            friends.push_back(PQgetvalue(res, i, 0));
        }
    }
    PQclear(res);
    return friends;
}
bool Database::SaveMessage(const std::string& sender, const std::string& target, const std::string& text) {
    if (!conn) return false;
    const char* params[3] = { sender.c_str(), target.c_str(), text.c_str() };
    std::string query = 
        "WITH users_ids AS ("
        "  SELECT "
        "    (SELECT id FROM users WHERE username = $1) as sid,"
        "    (SELECT id FROM users WHERE username = $2) as tid"
        "),"
        "target_room AS ("
        "  INSERT INTO dm_rooms (user_1, user_2)"
        "  SELECT LEAST(sid, tid), GREATEST(sid, tid) FROM users_ids"
        "  ON CONFLICT (user_1, user_2) DO UPDATE SET user_1 = EXCLUDED.user_1 "
        "  RETURNING id"
        ") "
        "INSERT INTO messages (room_id, sender_id, content) "
        "SELECT (SELECT id FROM target_room), sid, $3 FROM users_ids;";

    PGresult* res = PQexecParams(conn, query.c_str(), 3, NULL, params, NULL, NULL, 0);
    
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        std::cerr << "[DB] Ошибка сохранения сообщения/создания комнаты: " << PQerrorMessage(conn) << std::endl;
        PQclear(res);
        return false;
    }

    PQclear(res);
    return true;
}

std::vector<Message> Database::GetChatHistory(const std::string& u1, const std::string& u2, int offset, int limit) {
    std::vector<Message> history;
    const char* params[4] = { u1.c_str(), u2.c_str(), std::to_string(offset).c_str(), std::to_string(limit).c_str() };

    // Выбираем сообщения из нужной комнаты с лимитом и смещением
    std::string query = 
        "SELECT u.username, m.content, to_char(m.created_at, 'HH24:MI') "
        "FROM messages m "
        "JOIN users u ON m.sender_id = u.id "
        "JOIN dm_rooms r ON m.room_id = r.id "
        "WHERE (r.user_1 = (SELECT id FROM users WHERE username = $1) AND r.user_2 = (SELECT id FROM users WHERE username = $2)) "
        "OR (r.user_1 = (SELECT id FROM users WHERE username = $2) AND r.user_2 = (SELECT id FROM users WHERE username = $1)) "
        "ORDER BY m.created_at DESC "
        "LIMIT $4 OFFSET $3";

    PGresult* res = PQexecParams(conn, query.c_str(), 4, NULL, params, NULL, NULL, 0);
    if (PQresultStatus(res) == PGRES_TUPLES_OK) {
        for (int i = 0; i < PQntuples(res); i++) {
            Message m;
            m.sender = PQgetvalue(res, i, 0);
            m.text = PQgetvalue(res, i, 1);
            m.timeStr = PQgetvalue(res, i, 2);
            history.push_back(m);
        }
    }
    PQclear(res);
    return history; // Сообщения вернутся от новых к старым
}
