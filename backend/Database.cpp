#include "Database.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <cstring> // Для работы с C-строками, если понадобится
#include <vector>
#include <string>
#include <algorithm>

std::vector<std::string> Database::GetGroupMembers(const std::string& groupName) {
    std::vector<std::string> members;
    
    if (!conn) {
        std::cerr << "[DB GetGroupMembers] Нет подключения к БД!" << std::endl;
        return members;
    }
    
    std::cout << "[DB GetGroupMembers] Запрашиваем участников группы: '" << groupName << "'" << std::endl;
    
    const char* params[1] = { groupName.c_str() };
    
    PGresult* res = PQexecParams(conn, 
        "SELECT u.username FROM users u "
        "JOIN group_members gm ON u.id = gm.user_id "
        "JOIN groups g ON g.id = gm.group_id "
        "WHERE g.name = $1", 
        1, NULL, params, NULL, NULL, 0);

    if (PQresultStatus(res) == PGRES_TUPLES_OK) {
        int rows = PQntuples(res);
        std::cout << "[DB GetGroupMembers] Найдено участников: " << rows << std::endl;
        for (int i = 0; i < rows; i++) {
            std::string username = PQgetvalue(res, i, 0);
            members.push_back(username);
            std::cout << "[DB GetGroupMembers]   - " << username << std::endl;
        }
    } else {
        std::cerr << "[DB ERROR GetGroupMembers] " << PQerrorMessage(conn) << std::endl;
    }
    
    PQclear(res);
    return members;
}


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
std::string Database::Trim(const std::string& s) {
    if (s.empty()) return s;
    size_t first = s.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return "";
    size_t last = s.find_last_not_of(" \t\n\r");
    return s.substr(first, (last - first + 1));
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

    bool success = (PQresultStatus(res) == PGRES_COMMAND_OK);
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

bool Database::CreateGroup(const std::string& groupName, const std::vector<std::string>& members) {
    if (!conn) return false;

    std::string cleanGroupName = Trim(groupName);
    if (cleanGroupName.empty()) return false;

    std::cout << "[DB CreateGroup] Создаем группу: '" << cleanGroupName << "'" << std::endl;
    std::cout << "[DB CreateGroup] Участники (" << members.size() << "): ";
    for (const auto& m : members) std::cout << "'" << m << "' ";
    std::cout << std::endl;

    PQexec(conn, "BEGIN");

    // 1. Получаем ID всех участников по их именам (ИСКЛЮЧАЕМ ДУБЛИКАТЫ)
    std::vector<int> userIds;
    std::vector<std::string> uniqueNames;
    
    for (const auto& name : members) {
        std::string n = Trim(name);
        if (n.empty()) continue;
        
        // Проверяем, нет ли уже этого имени (регистронезависимо)
        bool alreadyExists = false;
        std::string lowerN = n;
        std::transform(lowerN.begin(), lowerN.end(), lowerN.begin(), ::tolower);
        
        for (const auto& existing : uniqueNames) {
            std::string lowerExisting = existing;
            std::transform(lowerExisting.begin(), lowerExisting.end(), lowerExisting.begin(), ::tolower);
            if (lowerN == lowerExisting) {
                alreadyExists = true;
                break;
            }
        }
        
        if (alreadyExists) {
            std::cout << "[DB CreateGroup] Пропускаем дубликат: '" << n << "'" << std::endl;
            continue;
        }
        
        uniqueNames.push_back(n);
        
        const char* p[1] = { n.c_str() };
        PGresult* resU = PQexecParams(conn, "SELECT id FROM users WHERE username = $1", 1, NULL, p, NULL, NULL, 0);
        if (PQntuples(resU) > 0) {
            int userId = atoi(PQgetvalue(resU, 0, 0));
            // Проверяем, нет ли уже этого ID
            if (std::find(userIds.begin(), userIds.end(), userId) == userIds.end()) {
                userIds.push_back(userId);
                std::cout << "[DB CreateGroup] Добавлен пользователь: '" << n << "' (ID: " << userId << ")" << std::endl;
            } else {
                std::cout << "[DB CreateGroup] Пропускаем дублирующийся ID: " << userId << std::endl;
            }
        } else {
            std::cout << "[DB CreateGroup] Пользователь не найден: '" << n << "'" << std::endl;
        }
        PQclear(resU);
    }

    if (userIds.empty()) {
        std::cout << "[DB CreateGroup] ОШИБКА: Нет валидных участников!" << std::endl;
        PQexec(conn, "ROLLBACK");
        return false;
    }

    std::cout << "[DB CreateGroup] Уникальных ID участников: " << userIds.size() << std::endl;

    // 2. Формируем строку массива для PostgreSQL: {1,2,3}
    std::string arrayStr = "{";
    for (size_t i = 0; i < userIds.size(); ++i) {
        arrayStr += std::to_string(userIds[i]);
        if (i < userIds.size() - 1) arrayStr += ",";
    }
    arrayStr += "}";

    std::cout << "[DB CreateGroup] Массив ID: " << arrayStr << std::endl;

    // 3. Создаем группу (вставляем и имя, и массив ID)
    const char* gParams[2] = { cleanGroupName.c_str(), arrayStr.c_str() };
    PGresult* resG = PQexecParams(conn,
        "INSERT INTO groups (name, member_ids) VALUES ($1, $2::integer[]) "
        "ON CONFLICT (name) DO UPDATE SET member_ids = EXCLUDED.member_ids "
        "RETURNING id",
        2, nullptr, gParams, nullptr, nullptr, 0
    );

    if (PQresultStatus(resG) != PGRES_TUPLES_OK) {
        std::cerr << "[DB ERROR] Ошибка создания группы: " << PQerrorMessage(conn) << std::endl;
        PQclear(resG);
        PQexec(conn, "ROLLBACK");
        return false;
    }
    
    int groupId = atoi(PQgetvalue(resG, 0, 0));
    std::cout << "[DB CreateGroup] Группа создана с ID: " << groupId << std::endl;
    PQclear(resG);

    // 4. Заполняем связующую таблицу group_members
    int insertedCount = 0;
    for (int uid : userIds) {
        std::string sid = std::to_string(groupId);
        std::string suid = std::to_string(uid);
        const char* mParams[2] = { sid.c_str(), suid.c_str() };
        
        PGresult* resM = PQexecParams(conn,
            "INSERT INTO group_members (group_id, user_id) VALUES ($1, $2) ON CONFLICT DO NOTHING",
            2, nullptr, mParams, nullptr, nullptr, 0
        );
        
        if (PQresultStatus(resM) == PGRES_COMMAND_OK) {
            if (atoi(PQcmdTuples(resM)) > 0) {
                insertedCount++;
            }
        }
        PQclear(resM);
    }

    std::cout << "[DB CreateGroup] Добавлено записей в group_members: " << insertedCount << std::endl;

    PQexec(conn, "COMMIT");
    std::cout << "[DB CreateGroup] Группа успешно создана!" << std::endl;
    
    // Проверим, что группа действительно создалась
    const char* checkParam[1] = { cleanGroupName.c_str() };
    PGresult* resCheck = PQexecParams(conn, "SELECT id, name, member_ids FROM groups WHERE name = $1", 
                                       1, NULL, checkParam, NULL, NULL, 0);
    if (PQntuples(resCheck) > 0) {
        std::cout << "[DB CreateGroup] Проверка: группа существует в БД, ID=" 
                  << PQgetvalue(resCheck, 0, 0) 
                  << ", name='" << PQgetvalue(resCheck, 0, 1) << "'" << std::endl;
    }
    PQclear(resCheck);
    
    return true;
}

bool Database::IsGroup(const std::string& name) {
    if (!conn) return false;
    
    std::string cleanName = Trim(name);
    if (cleanName.empty()) return false;
    
    std::cout << "[DB IsGroup] Проверяем: '" << cleanName << "'" << std::endl;
    
    // Ищем точное совпадение
    const char* exactParam[1] = { cleanName.c_str() };
    PGresult* resExact = PQexecParams(conn, 
        "SELECT id FROM groups WHERE name = $1", 
        1, NULL, exactParam, NULL, NULL, 0);
    
    int exactCount = PQntuples(resExact);
    if (exactCount > 0) {
        std::cout << "[DB IsGroup] Найдено точное совпадение, ID: " << PQgetvalue(resExact, 0, 0) << std::endl;
        PQclear(resExact);
        return true;
    }
    PQclear(resExact);
    
    std::string searchPattern = cleanName.substr(0, 10) + "%"; 
    const char* likeParam[1] = { searchPattern.c_str() };
    PGresult* resLike = PQexecParams(conn, 
        "SELECT id, name FROM groups WHERE name LIKE $1", 
        1, NULL, likeParam, NULL, NULL, 0);
    
    int likeCount = PQntuples(resLike);
    if (likeCount > 0) {
        std::cout << "[DB IsGroup] Найдено частичное совпадение:" << std::endl;
        for (int i = 0; i < likeCount; i++) {
            std::cout << "  - ID: " << PQgetvalue(resLike, i, 0) 
                      << ", name: '" << PQgetvalue(resLike, i, 1) << "'" << std::endl;
        }
        PQclear(resLike);
        return true;
    }
    
    std::cout << "[DB IsGroup] Группа не найдена!" << std::endl;
    PQclear(resLike);
    return false;
}
std::vector<std::string> Database::GetUserGroups(const std::string& username) {
    std::vector<std::string> groups;
    const char* params[1] = { username.c_str() };
    
    PGresult* res = PQexecParams(conn,
        "SELECT g.name FROM groups g "
        "JOIN group_members gm ON g.id = gm.group_id " 
        "JOIN users u ON u.id = gm.user_id "
        "WHERE u.username = $1", 1, NULL, params, NULL, NULL, 0);

    if (PQresultStatus(res) == PGRES_TUPLES_OK) {
        for (int i = 0; i < PQntuples(res); i++) {
            groups.push_back(PQgetvalue(res, i, 0));
        }
    }
    PQclear(res);
    return groups;
}

bool Database::SaveMessage(const std::string& sender, const std::string& target, const std::string& text) {
    if (!conn) return false;

    std::string cleanSender = Trim(sender);
    std::string cleanTarget = Trim(target);
    std::string cleanText = text; // Не обрезаем текст, только пробелы по краям
    
    // Удаляем пробелы только по краям
    cleanText.erase(0, cleanText.find_first_not_of(" \t\n\r"));
    cleanText.erase(cleanText.find_last_not_of(" \t\n\r") + 1);
    
    if (cleanText.empty()) {
        std::cout << "[DB SaveMessage] Пустой текст, пропускаем" << std::endl;
        return false;
    }
    
    std::cout << "[DB SaveMessage] =============== НАЧАЛО СОХРАНЕНИЯ ===============" << std::endl;
    std::cout << "[DB SaveMessage] Отправитель: '" << cleanSender << "'" << std::endl;
    std::cout << "[DB SaveMessage] Цель: '" << cleanTarget << "'" << std::endl;
    std::cout << "[DB SaveMessage] Текст (первые 100 символов): '" << cleanText.substr(0, 100) << "'" << std::endl;
    std::cout << "[DB SaveMessage] Длина текста: " << cleanText.length() << std::endl;

    // Проверяем, является ли цель группой
    bool isGroupTarget = IsGroup(cleanTarget);
    std::cout << "[DB SaveMessage] Это группа? " << (isGroupTarget ? "ДА" : "НЕТ") << std::endl;
    
    if (isGroupTarget) {
        std::cout << "[DB SaveMessage] === СОХРАНЕНИЕ В ГРУППУ ===" << std::endl;
        
        // Сначала найдем точное имя группы в БД
        std::string actualGroupName = "";
        const char* findParam[1] = { cleanTarget.c_str() };
        PGresult* resFind = PQexecParams(conn,
            "SELECT name FROM groups WHERE name = $1",
            1, NULL, findParam, NULL, NULL, 0);
            
        if (PQntuples(resFind) > 0) {
            actualGroupName = PQgetvalue(resFind, 0, 0);
            std::cout << "[DB SaveMessage] Найдено точное имя группы: '" << actualGroupName << "'" << std::endl;
        } else {
            // Если не нашли точное, ищем частичное
            PQclear(resFind);
            std::string pattern = cleanTarget;
            if (pattern.length() > 10) pattern = pattern.substr(0, 10);
            pattern += "%";
            
            const char* likeParam[1] = { pattern.c_str() };
            resFind = PQexecParams(conn,
                "SELECT name FROM groups WHERE name LIKE $1 LIMIT 1",
                1, NULL, likeParam, NULL, NULL, 0);
                
            if (PQntuples(resFind) > 0) {
                actualGroupName = PQgetvalue(resFind, 0, 0);
                std::cout << "[DB SaveMessage] Найдено частичное совпадение: '" << actualGroupName << "'" << std::endl;
            }
        }
        
        if (actualGroupName.empty()) {
            std::cerr << "[DB SaveMessage] ОШИБКА: Не удалось найти имя группы для '" << cleanTarget << "'" << std::endl;
            PQclear(resFind);
            return false;
        }
        
        PQclear(resFind);
        
        // Проверим, существует ли отправитель
        const char* checkUser[1] = { cleanSender.c_str() };
        PGresult* resUser = PQexecParams(conn, "SELECT id FROM users WHERE username = $1", 
                                          1, NULL, checkUser, NULL, NULL, 0);
        if (PQntuples(resUser) == 0) {
            std::cerr << "[DB SaveMessage] ОШИБКА: Отправитель '" << cleanSender << "' не найден!" << std::endl;
            PQclear(resUser);
            return false;
        }
        std::string senderId = PQgetvalue(resUser, 0, 0);
        std::cout << "[DB SaveMessage] ID отправителя: " << senderId << std::endl;
        PQclear(resUser);
        
        // Параметры для вставки
        const char* params[3] = { actualGroupName.c_str(), cleanSender.c_str(), cleanText.c_str() };
        
        std::cout << "[DB SaveMessage] Параметры SQL:" << std::endl;
        std::cout << "[DB SaveMessage]   - Группа: " << actualGroupName << std::endl;
        std::cout << "[DB SaveMessage]   - Отправитель: " << cleanSender << std::endl;
        std::cout << "[DB SaveMessage]   - Текст: " << cleanText.substr(0, 50) << "..." << std::endl;
        
        std::string query = 
            "INSERT INTO group_messages (group_id, sender_id, content) "
            "SELECT g.id, u.id, $3 "
            "FROM groups g, users u "
            "WHERE g.name = $1 AND u.username = $2 "
            "RETURNING id";  // Добавим RETURNING для отладки
        
        std::cout << "[DB SaveMessage] Выполняем запрос: " << std::endl;
        std::cout << "[DB SaveMessage] " << query << std::endl;
        
        PGresult* res = PQexecParams(conn, query.c_str(), 3, NULL, params, NULL, NULL, 0);
        
        if (PQresultStatus(res) != PGRES_TUPLES_OK) {
            std::cerr << "[DB SaveMessage] ОШИБКА SQL: " << PQerrorMessage(conn) << std::endl;
            PQclear(res);
            return false;
        }

        int rows = PQntuples(res);
        std::cout << "[DB SaveMessage] Запрос выполнен успешно!" << std::endl;
        std::cout << "[DB SaveMessage] Возвращено строк: " << rows << std::endl;
        
        if (rows > 0) {
            std::string msgId = PQgetvalue(res, 0, 0);
            std::cout << "[DB SaveMessage] Сообщение сохранено с ID: " << msgId << std::endl;
        }
        
        // Проверим количество затронутых строк
        const char* cmdTuples = PQcmdTuples(res);
        if (cmdTuples && strlen(cmdTuples) > 0) {
            int affected = atoi(cmdTuples);
            std::cout << "[DB SaveMessage] Затронуто строк (PQcmdTuples): " << affected << std::endl;
        } else {
            std::cout << "[DB SaveMessage] PQcmdTuples вернул NULL или пустую строку" << std::endl;
        }
        
        PQclear(res);
        
        // Дополнительная проверка: посчитаем сколько всего сообщений в группе
        const char* countParam[1] = { actualGroupName.c_str() };
        PGresult* resCount = PQexecParams(conn,
            "SELECT COUNT(*) FROM group_messages WHERE group_id = (SELECT id FROM groups WHERE name = $1)",
            1, NULL, countParam, NULL, NULL, 0);
            
        if (PQntuples(resCount) > 0) {
            std::cout << "[DB SaveMessage] Всего сообщений в группе теперь: " << PQgetvalue(resCount, 0, 0) << std::endl;
        }
        PQclear(resCount);
        
        std::cout << "[DB SaveMessage] === СОХРАНЕНИЕ ЗАВЕРШЕНО ===" << std::endl;
        std::cout << "[DB SaveMessage] ============================================" << std::endl;
        
        return true;
    }
    else {
        std::cout << "[DB SaveMessage] === СОХРАНЕНИЕ ЛИЧНОГО СООБЩЕНИЯ ===" << std::endl;
        
        const char* params[3] = { cleanSender.c_str(), cleanTarget.c_str(), cleanText.c_str() };
        
        std::string query = 
            "WITH users_ids AS ("
            "  SELECT "
            "    (SELECT id FROM users WHERE username = $1) as sid,"
            "    (SELECT id FROM users WHERE username = $2) as tid"
            "),"
            "target_room AS ("
            "  INSERT INTO dm_rooms (user_1, user_2)"
            "  SELECT LEAST(sid, tid), GREATEST(sid, tid) FROM users_ids"
            "  WHERE sid IS NOT NULL AND tid IS NOT NULL "
            "  ON CONFLICT (user_1, user_2) DO UPDATE SET user_1 = EXCLUDED.user_1 "
            "  RETURNING id"
            ") "
            "INSERT INTO messages (room_id, sender_id, content) "
            "SELECT (SELECT id FROM target_room), sid, $3 FROM users_ids "
            "WHERE sid IS NOT NULL AND (SELECT id FROM target_room) IS NOT NULL "
            "RETURNING id";

        PGresult* res = PQexecParams(conn, query.c_str(), 3, NULL, params, NULL, NULL, 0);
        
        if (PQresultStatus(res) != PGRES_TUPLES_OK) {
            std::cerr << "[DB SaveMessage DM] ОШИБКА: " << PQerrorMessage(conn) << std::endl;
            PQclear(res);
            return false;
        }
        
        int rows = PQntuples(res);
        std::cout << "[DB SaveMessage DM] Сохранено строк: " << rows << std::endl;
        
        if (rows > 0) {
            std::cout << "[DB SaveMessage DM] ID сообщения: " << PQgetvalue(res, 0, 0) << std::endl;
        }
        
        PQclear(res);
        
        std::cout << "[DB SaveMessage] === ЛИЧНОЕ СООБЩЕНИЕ СОХРАНЕНО ===" << std::endl;
        std::cout << "[DB SaveMessage] ==================================" << std::endl;
        
        return true;
    }
}

std::vector<Message> Database::GetChatHistory(const std::string& u1, const std::string& u2, int offset, int limit) {
    std::vector<Message> history;
    if (!conn) return history;

    PGresult* res = nullptr;

    if (IsGroup(u2)) {
        // --- ЗАГРУЗКА ИЗ ГРУППЫ ---
        const char* params[3] = { u2.c_str(), std::to_string(offset).c_str(), std::to_string(limit).c_str() };
        std::string query = 
            "SELECT u.username, gm.content, to_char(gm.created_at, 'HH24:MI') "
            "FROM group_messages gm "
            "JOIN users u ON gm.sender_id = u.id "
            "JOIN groups g ON gm.group_id = g.id "
            "WHERE g.name = $1 "
            "ORDER BY gm.created_at DESC "
            "LIMIT $3 OFFSET $2";
        res = PQexecParams(conn, query.c_str(), 3, NULL, params, NULL, NULL, 0);
    } else {
        // --- ЗАГРУЗКА ИЗ ЛИЧКИ (DM) ---
        const char* params[4] = { u1.c_str(), u2.c_str(), std::to_string(offset).c_str(), std::to_string(limit).c_str() };
        std::string query = 
            "SELECT u.username, m.content, to_char(m.created_at, 'HH24:MI') "
            "FROM messages m "
            "JOIN users u ON m.sender_id = u.id "
            "JOIN dm_rooms r ON m.room_id = r.id "
            "WHERE (r.user_1 = (SELECT id FROM users WHERE username = $1) AND r.user_2 = (SELECT id FROM users WHERE username = $2)) "
            "OR (r.user_1 = (SELECT id FROM users WHERE username = $2) AND r.user_2 = (SELECT id FROM users WHERE username = $1)) "
            "ORDER BY m.created_at DESC "
            "LIMIT $4 OFFSET $3";
        res = PQexecParams(conn, query.c_str(), 4, NULL, params, NULL, NULL, 0);
    }

    if (res && PQresultStatus(res) == PGRES_TUPLES_OK) {
        for (int i = 0; i < PQntuples(res); i++) {
            Message m;
            m.sender = PQgetvalue(res, i, 0);
            m.text = PQgetvalue(res, i, 1);
            m.timeStr = PQgetvalue(res, i, 2);
            history.push_back(m);
        }
    } else {
        std::cerr << "[DB ERROR History] " << PQerrorMessage(conn) << std::endl;
    }

    if (res) PQclear(res);
    return history;
}
