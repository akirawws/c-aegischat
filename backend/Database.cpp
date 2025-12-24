#include "Database.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>

// Вспомогательная функция для загрузки переменных окружения
std::map<std::string, std::string> LoadEnv(const std::string& filename) {
    std::map<std::string, std::string> env;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "Критическая ошибка: Не удалось открыть файл " << filename << std::endl;
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
                // Удаляем возможные лишние пробелы по краям
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

Database::Database() : conn(nullptr) {
}

Database::~Database() {
    Disconnect();
}

bool Database::Connect() {
    auto env = LoadEnv(".env");

    if (env.empty()) {
        std::cerr << "Ошибка: .env файл пуст или отсутствует!" << std::endl;
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
        std::cerr << "Ошибка подключения к БД: " << PQerrorMessage(conn) << std::endl;
        return false;
    }

    std::cout << "Успешное подключение к PostgreSQL (данные загружены из .env)!" << std::endl;
    return true;
}

void Database::Disconnect() {
    if (conn) {
        PQfinish(conn);
        conn = nullptr;
    }
}
bool Database::Execute(const std::string& query) {
    PGresult* res = PQexec(conn, query.c_str());
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        std::cerr << "Ошибка выполнения команды: " << PQerrorMessage(conn) << std::endl;
        PQclear(res);
        return false;
    }
    PQclear(res);
    return true;
}

PGresult* Database::Query(const std::string& query) {
    PGresult* res = PQexec(conn, query.c_str());
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        std::cerr << "Ошибка запроса: " << PQerrorMessage(conn) << std::endl;
        PQclear(res);
        return nullptr;
    }
    return res;
}