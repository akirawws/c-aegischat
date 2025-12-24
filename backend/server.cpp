#include <iostream>
#include <vector>
#include <string>
#include <winsock2.h>
#include <thread>
#include <mutex>
#include <algorithm>
#include "Database.h" 
#include "AuthProtocol.h" // Обязательно подключаем протокол

#pragma comment(lib, "ws2_32.lib")

std::vector<SOCKET> clients;
std::mutex clients_mutex;
Database db; 

void HandleClient(SOCKET client_socket) {
    char buffer[sizeof(AuthPacket) + 100]; // Запас под размер пакета
    
    while (true) {
        ZeroMemory(buffer, sizeof(buffer));
        int bytesReceived = recv(client_socket, buffer, sizeof(buffer), 0);

        if (bytesReceived <= 0) {
            std::cout << "[!] Клиент отключился." << std::endl;
            break;
        }

        // 1. Проверяем, является ли входящий пакет структурой авторизации
        AuthPacket* packet = (AuthPacket*)buffer;

        if (packet->type == PACKET_LOGIN || packet->type == PACKET_REGISTER) {
            ResponsePacket res = { PACKET_AUTH_RESPONSE, false, "" };

            if (packet->type == PACKET_REGISTER) {
                std::cout << "[REG] Запрос от: " << packet->username << std::endl;
                if (db.RegisterUser(packet->username, packet->email, packet->password)) {
                    res.success = true;
                    strcpy(res.message, "Account created successfully!");
                } else {
                    strcpy(res.message, "User already exists or DB error.");
                }
            } 
            else if (packet->type == PACKET_LOGIN) {
                std::cout << "[AUTH] Вход: " << packet->username << std::endl;
                if (db.AuthenticateUser(packet->username, packet->password)) {
                    res.success = true;
                    strcpy(res.message, "Welcome to AEGIS.");
                } else {
                    strcpy(res.message, "Invalid username or password.");
                }
            }

            // Отправляем ответ клиенту
            send(client_socket, (char*)&res, sizeof(ResponsePacket), 0);
        } 
        else {
            // Если это не пакет авторизации, считаем это обычным сообщением чата (для Broadcast)
            std::string msg(buffer);
            std::cout << "[CHAT] " << msg << std::endl;
            
            std::lock_guard<std::mutex> lock(clients_mutex);
            for (SOCKET s : clients) {
                if (s != client_socket) {
                    send(s, buffer, bytesReceived, 0);
                }
            }
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