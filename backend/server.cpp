#include <iostream>
#include <vector>
#include <string>
#include <winsock2.h>
#include <thread>
#include <mutex>
#include <algorithm>
#include "Database.h" // Подключаем твой класс БД

#pragma comment(lib, "ws2_32.lib")

std::vector<SOCKET> clients;
std::mutex clients_mutex;
Database db; // Глобальный объект базы данных

void Broadcast(const std::string& message, SOCKET sender_socket) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (auto it = clients.begin(); it != clients.end(); ) {
        int res = send(*it, message.c_str(), (int)message.size() + 1, 0);
        if (res == SOCKET_ERROR) {
            closesocket(*it);
            it = clients.erase(it);
        } else {
            ++it;
        }
    }
}

void HandleClient(SOCKET client_socket) {
    char buffer[4096];
    while (true) {
        ZeroMemory(buffer, 4096);
        int bytesReceived = recv(client_socket, buffer, 4096, 0);

        if (bytesReceived <= 0) {
            std::cout << "[!] Клиент отключился." << std::endl;
            break;
        }

        std::string msg(buffer);
        std::cout << "[LOG] Получено: " << msg << std::endl;

        if (msg.find("AUTH:") == 0) {
             std::cout << "[AUTH] Попытка входа..." << std::endl;
        } 
        else if (msg.find("REG:") == 0) {
             std::cout << "[REG] Регистрация нового юзера..." << std::endl;
        }
        else {
            Broadcast(msg, client_socket);
        }
    }

    std::lock_guard<std::mutex> lock(clients_mutex);
    clients.erase(std::remove(clients.begin(), clients.end(), client_socket), clients.end());
    closesocket(client_socket);
}

int main() {
    SetConsoleCP(65001);
    SetConsoleOutputCP(65001);

    if (!db.Connect()) {
        std::cerr << "[CRITICAL] Не удалось подключиться к БД! Проверьте .env" << std::endl;
        return 1; 
    }

    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in hint;
    hint.sin_family = AF_INET;
    hint.sin_port = htons(5555);
    hint.sin_addr.S_un.S_addr = INADDR_ANY;

    bind(serverSocket, (sockaddr*)&hint, sizeof(hint));
    listen(serverSocket, SOMAXCONN);

    std::cout << "[SERVER] AEGIS Backend запущен на порту 5555" << std::endl;

    while (true) {
        SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            clients.push_back(clientSocket);
        }
        std::thread(HandleClient, clientSocket).detach();
    }

    WSACleanup();
    return 0;
}