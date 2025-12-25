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
    std::string currentUsername = ""; // Храним ник этого клиента для удобства

    while (true) {
        ZeroMemory(buffer, sizeof(buffer));
        int bytesReceived = recv(client_socket, buffer, sizeof(buffer), 0);

        if (bytesReceived <= 0) {
            std::cout << "[!] Клиент [" << (currentUsername.empty() ? "Неизвестно" : currentUsername) << "] отключился." << std::endl;
            break;
        }

        uint8_t packetType = *(uint8_t*)buffer;

        // 1. АВТОРИЗАЦИЯ И РЕГИСТРАЦИЯ
        if (packetType == PACKET_LOGIN || packetType == PACKET_REGISTER) {
            AuthPacket* packet = (AuthPacket*)buffer;
            ResponsePacket res = { PACKET_AUTH_RESPONSE, false, "" };

            if (packet->type == PACKET_REGISTER) {
                if (db.RegisterUser(packet->username, packet->email, packet->password)) {
                    res.success = true;
                    strcpy(res.message, "Success!");
                } else {
                    strcpy(res.message, "Error!");
                }
                send(client_socket, (char*)&res, sizeof(ResponsePacket), 0);
            } 
            else if (packet->type == PACKET_LOGIN) {
                if (db.AuthenticateUser(packet->username, packet->password)) {
                    currentUsername = packet->username; 
                    res.success = true;
                    strcpy(res.message, "Welcome!");

                    {
                        std::lock_guard<std::mutex> lock(users_mutex);
                        onlineUsers[currentUsername] = client_socket;
                    }
                    send(client_socket, (char*)&res, sizeof(ResponsePacket), 0);

                    // Оффлайн-заявки
                    std::vector<std::string> pending = db.GetPendingRequests(currentUsername);
                    for (const auto& senderName : pending) {
                        FriendPacket fp = { PACKET_FRIEND_REQUEST };
                        strncpy(fp.senderUsername, senderName.c_str(), 31);
                        strncpy(fp.targetUsername, currentUsername.c_str(), 31);
                        send(client_socket, (char*)&fp, sizeof(FriendPacket), 0);
                    }
                } else {
                    strcpy(res.message, "Failed!");
                    send(client_socket, (char*)&res, sizeof(ResponsePacket), 0);
                }
            }
        } 
        // 2. НОВАЯ ЗАЯВКА В ДРУЗЬЯ
        else if (packetType == PACKET_FRIEND_REQUEST) {
            FriendPacket* fPkt = (FriendPacket*)buffer;
            if (db.AddFriendRequest(fPkt->senderUsername, fPkt->targetUsername)) {
                std::lock_guard<std::mutex> lock(users_mutex);
                if (onlineUsers.count(fPkt->targetUsername)) {
                    send(onlineUsers[fPkt->targetUsername], (char*)fPkt, sizeof(FriendPacket), 0);
                }
            }
        }
        // 3. ПРИНЯТЬ ИЛИ ОТКЛОНИТЬ ЗАЯВКУ (Добавлено!)
        else if (packetType == PACKET_FRIEND_ACCEPT || packetType == PACKET_FRIEND_REJECT) {
            // Важно: приведение (FriendActionPacket*)buffer должно быть обернуто в скобки
            FriendActionPacket* aPkt = (FriendActionPacket*)buffer;
            bool isAccept = (packetType == PACKET_FRIEND_ACCEPT);

            if (db.HandleFriendAction(aPkt->targetUsername, currentUsername, isAccept)) {
                std::cout << "[FRIEND] " << currentUsername 
                        << (isAccept ? " принял " : " отклонил ") 
                        << "заявку от " << aPkt->targetUsername << std::endl;
            }
        }
        // 4. ОБЫЧНЫЙ ЧАТ
        else {
            std::lock_guard<std::mutex> lock(clients_mutex);
            for (SOCKET s : clients) {
                if (s != client_socket) send(s, buffer, bytesReceived, 0);
            }
        }
    }

    // Очистка при выходе
    if (!currentUsername.empty()) {
        std::lock_guard<std::mutex> lock(users_mutex);
        onlineUsers.erase(currentUsername);
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