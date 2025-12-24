#include <iostream>
#include <vector>
#include <string>
#include <winsock2.h>
#include <thread>
#include <mutex>
#include <algorithm>
#include "Database.h" 
#include "AuthProtocol.h" 

#pragma comment(lib, "ws2_32.lib")

std::vector<SOCKET> clients;
std::mutex clients_mutex;
Database db; 

void HandleClient(SOCKET client_socket) {
    // Используем размер самого большого пакета + запас
    char buffer[512]; 
    
    while (true) {
        ZeroMemory(buffer, sizeof(buffer));
        int bytesReceived = recv(client_socket, buffer, sizeof(buffer), 0);

        if (bytesReceived <= 0) {
            std::cout << "[!] Клиент отключился." << std::endl;
            break;
        }

        // Все пакеты начинаются с поля type (uint8_t)
        uint8_t packetType = *(uint8_t*)buffer;

        if (packetType == PACKET_LOGIN || packetType == PACKET_REGISTER) {
            AuthPacket* packet = (AuthPacket*)buffer;
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
            send(client_socket, (char*)&res, sizeof(ResponsePacket), 0);
        } 
        else if (packetType == PACKET_FRIEND_REQUEST) {
            FriendPacket* fPkt = (FriendPacket*)buffer;
            std::cout << "[FRIEND] Запрос: " << fPkt->senderUsername << " -> " << fPkt->targetUsername << std::endl;

            // Логика БД: вернет true, если запись вставлена
            if (db.AddFriendRequest(fPkt->senderUsername, fPkt->targetUsername)) {
                std::cout << "[SUCCESS] Заявка сохранена в БД." << std::endl;
            } else {
                std::cerr << "[ERROR] Ошибка БД (юзер не найден или уже в друзьях)." << std::endl;
            }
            // Здесь можно отправить ResponsePacket отправителю, чтобы клиент вывел "Запрос отправлен"
        }
        else {
            // Обычный чат / Broadcast
            std::cout << "[CHAT] Сообщение получено." << std::endl;
            std::lock_guard<std::mutex> lock(clients_mutex);
            for (SOCKET s : clients) {
                if (s != client_socket) {
                    send(s, buffer, bytesReceived, 0);
                }
            }
        }
    }

    std::lock_guard<std::mutex> lock(clients_mutex);
    auto it = std::find(clients.begin(), clients.end(), client_socket);
    if (it != clients.end()) clients.erase(it);
    closesocket(client_socket);
}

int main() {
    SetConsoleCP(65001);
    SetConsoleOutputCP(65001);

    if (!db.Connect()) {
        std::cerr << "[CRITICAL] Не удалось подключиться к БД!" << std::endl;
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

    closesocket(serverSocket);
    WSACleanup();
    return 0;
}