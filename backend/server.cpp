#include <iostream>
#include <vector>
#include <string>
#include <winsock2.h>
#include <thread>
#include <mutex>
#include <algorithm>
#include "Database.h" 
#include "AuthProtocol.h" 
#include <map>

#pragma comment(lib, "ws2_32.lib")

// Глобальные переменные
std::vector<SOCKET> clients;
std::mutex clients_mutex;
Database db; 

std::map<std::string, SOCKET> onlineUsers; // Ник -> Сокет
std::mutex users_mutex;

void HandleClient(SOCKET client_socket) {
    char buffer[512]; 
    
    while (true) {
        ZeroMemory(buffer, sizeof(buffer));
        int bytesReceived = recv(client_socket, buffer, sizeof(buffer), 0);

        if (bytesReceived <= 0) {
            std::cout << "[!] Клиент отключился." << std::endl;
            break;
        }

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
                send(client_socket, (char*)&res, sizeof(ResponsePacket), 0);
            } 
            else if (packet->type == PACKET_LOGIN) {
                std::cout << "[AUTH] Вход: " << packet->username << std::endl;
                std::string currentUsername = packet->username; 

                if (db.AuthenticateUser(currentUsername, packet->password)) {
                    res.success = true;
                    strcpy(res.message, "Welcome to AEGIS.");

                    {
                        std::lock_guard<std::mutex> lock(users_mutex);
                        onlineUsers[currentUsername] = client_socket;
                    }

                    send(client_socket, (char*)&res, sizeof(ResponsePacket), 0);

                    // Рассылка оффлайн-заявок
                    std::vector<std::string> pending = db.GetPendingRequests(currentUsername);
                    for (const auto& senderName : pending) {
                        FriendPacket fp;
                        fp.type = PACKET_FRIEND_REQUEST;
                        memset(fp.senderUsername, 0, 32);
                        memset(fp.targetUsername, 0, 32);
                        strncpy(fp.senderUsername, senderName.c_str(), 31);
                        strncpy(fp.targetUsername, currentUsername.c_str(), 31);
                        send(client_socket, (char*)&fp, sizeof(FriendPacket), 0);
                    }
                } else {
                    strcpy(res.message, "Invalid username or password.");
                    send(client_socket, (char*)&res, sizeof(ResponsePacket), 0);
                }
            }
        } 
        else if (packetType == PACKET_FRIEND_REQUEST) {
            FriendPacket* fPkt = (FriendPacket*)buffer;
            if (db.AddFriendRequest(fPkt->senderUsername, fPkt->targetUsername)) {
                std::cout << "[SUCCESS] Заявка сохранена в БД." << std::endl;

                std::lock_guard<std::mutex> lock(users_mutex);
                if (onlineUsers.count(fPkt->targetUsername)) {
                    SOCKET targetSock = onlineUsers[fPkt->targetUsername];
                    send(targetSock, (char*)fPkt, sizeof(FriendPacket), 0);
                    std::cout << "[NET] Переслано юзеру " << fPkt->targetUsername << std::endl;
                }
            } else {
                std::cerr << "[ERROR] Ошибка БД при добавлении друга." << std::endl;
            }
        }
        else {
            // Broadcast чат
            std::lock_guard<std::mutex> lock(clients_mutex);
            for (SOCKET s : clients) {
                if (s != client_socket) {
                    send(s, buffer, bytesReceived, 0);
                }
            }
        }
    }

    // Очистка при выходе
    {
        std::lock_guard<std::mutex> lock(users_mutex);
        for (auto it = onlineUsers.begin(); it != onlineUsers.end(); ) {
            if (it->second == client_socket) it = onlineUsers.erase(it);
            else ++it;
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

    if (!db.Connect()) return 1;

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