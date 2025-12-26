#include <iostream>
#include <vector>
#include <string>
#include <winsock2.h>
#include <thread>
#include <mutex>
#include <algorithm>
#include <map>
#include "Database.h" 
#include "AuthProtocol.h" 

#pragma comment(lib, "ws32_lib")

std::vector<SOCKET> clients;
std::mutex clients_mutex;
Database db; 

std::map<std::string, SOCKET> onlineUsers; 
std::mutex users_mutex;
void BroadcastStatusToFriends(const std::string& username, uint8_t status) {
    // Получаем список друзей из БД
    std::vector<std::string> friends = db.GetAcceptedFriends(username);
    
    UserStatusPacket sPkt;
    sPkt.type = PACKET_USER_STATUS;
    strncpy(sPkt.username, username.c_str(), 63);
    sPkt.onlineStatus = status;

    std::lock_guard<std::mutex> lock(users_mutex);
    for (const auto& fName : friends) {
        // Если друг онлайн — отправляем ему пакет
        if (onlineUsers.count(fName)) {
            send(onlineUsers[fName], (char*)&sPkt, sizeof(UserStatusPacket), 0);
        }
    }
}


void HandleClient(SOCKET client_socket) {
    char buffer[512]; 
    std::string currentUsername = ""; 

    while (true) {
        ZeroMemory(buffer, sizeof(buffer));
        int bytesReceived = recv(client_socket, buffer, sizeof(buffer), 0);

        if (bytesReceived <= 0) break;

        uint8_t packetType = *(uint8_t*)buffer;

        // --- 1. АВТОРИЗАЦИЯ ---
        if (packetType == PACKET_LOGIN || packetType == PACKET_REGISTER) {
            AuthPacket* packet = (AuthPacket*)buffer;
            ResponsePacket res = { PACKET_AUTH_RESPONSE, false, "" };

            if (packet->type == PACKET_REGISTER) {
                if (db.RegisterUser(packet->username, packet->email, packet->password)) {
                    res.success = true;
                    strcpy(res.message, "Registered!");
                } else {
                    strcpy(res.message, "User exists!");
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

                // 1. Оповещаем друзей, что мы теперь ОНЛАЙН
                BroadcastStatusToFriends(currentUsername, 1);

                // 2. Отправляем пользователю список его друзей с их ТЕКУЩИМ статусом
                std::vector<std::string> friends = db.GetAcceptedFriends(currentUsername);
                for (const auto& fName : friends) {
                    RoomPacket rPkt;
                    rPkt.type = PACKET_ROOM_LIST;
                    memset(rPkt.username, 0, 64);
                    strncpy(rPkt.username, fName.c_str(), 63);
                    
                    // Проверяем, онлайн ли этот друг прямо сейчас
                    {
                        std::lock_guard<std::mutex> lock(users_mutex);
                        rPkt.onlineStatus = (onlineUsers.count(fName) > 0) ? 1 : 0;
                    }

                    send(client_socket, (char*)&rPkt, sizeof(RoomPacket), 0);
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }

                    // Отправляем оффлайн-заявки
                    std::vector<std::string> pending = db.GetPendingRequests(currentUsername);
                    for (const auto& senderName : pending) {
                        FriendPacket fp = { PACKET_FRIEND_REQUEST };
                        strncpy(fp.senderUsername, senderName.c_str(), 31);
                        strncpy(fp.targetUsername, currentUsername.c_str(), 31);
                        send(client_socket, (char*)&fp, sizeof(FriendPacket), 0);
                    }
                } else {
                    strcpy(res.message, "Invalid credentials!");
                    send(client_socket, (char*)&res, sizeof(ResponsePacket), 0);
                }
            }
        } 
        // --- 2. ОБРАБОТКА ЗАЯВОК ---
        else if (packetType == PACKET_FRIEND_REQUEST) {
            FriendPacket* fPkt = (FriendPacket*)buffer;
            if (db.AddFriendRequest(fPkt->senderUsername, fPkt->targetUsername)) {
                std::lock_guard<std::mutex> lock(users_mutex);
                if (onlineUsers.count(fPkt->targetUsername)) {
                    send(onlineUsers[fPkt->targetUsername], (char*)fPkt, sizeof(FriendPacket), 0);
                }
            }
        }
                else if (packetType == PACKET_FRIEND_ACCEPT) {
                    FriendActionPacket* aPkt = (FriendActionPacket*)buffer;
                    
                    // В БД сохраняем: currentUsername (кто нажал ок), aPkt->targetUsername (кто кидал)
                    if (db.AcceptFriendAndCreateRoom(aPkt->targetUsername, currentUsername)) {
                        std::lock_guard<std::mutex> lock(users_mutex);
                        
                        // 1. Уведомляем отправителя заявки (User A), если он в сети
                        if (onlineUsers.count(aPkt->targetUsername)) {
                            FriendActionPacket notification = { PACKET_FRIEND_ACCEPT };
                            strncpy(notification.targetUsername, currentUsername.c_str(), 31);
                            send(onlineUsers[aPkt->targetUsername], (char*)&notification, sizeof(FriendActionPacket), 0);
                        }

                        // 2. Уведомляем того, кто принял (User B) — подтверждаем успех
                        // Это гарантирует, что у обоих выполнится AddUserToDMList через ReceiveMessages
                        FriendActionPacket confirm = { PACKET_FRIEND_ACCEPT };
                        strncpy(confirm.targetUsername, aPkt->targetUsername, 31);
                        send(client_socket, (char*)&confirm, sizeof(FriendActionPacket), 0);
                    }
                }
                
        else if (packetType == PACKET_FRIEND_REJECT) {
            FriendActionPacket* aPkt = (FriendActionPacket*)buffer;
            db.HandleFriendAction(aPkt->targetUsername, currentUsername, false);
        }
        // --- 3. ЧАТ ---
    else if (packetType == PACKET_CHAT_MESSAGE) {
        ChatMessagePacket* cPkt = (ChatMessagePacket*)buffer;

        // Безопасно обрываем строки
        cPkt->senderUsername[63] = '\0';
        cPkt->targetUsername[63] = '\0';
        cPkt->content[383] = '\0';

        std::string sender = cPkt->senderUsername;
        std::string target = cPkt->targetUsername;
        std::string text   = cPkt->content;

        // 🔒 ВАЖНО: защита от подмены
        if (sender != currentUsername) {
            std::cout << "[SECURITY] sender spoofing blocked\n";
            continue;
        }

        // 💾 СОХРАНЯЕМ В БД
        db.SaveMessage(sender, target, text);

        // 📤 ОТПРАВЛЯЕМ ПОЛУЧАТЕЛЮ (НЕ МЕНЯЯ ПАКЕТ)
        std::lock_guard<std::mutex> lock(users_mutex);
        if (onlineUsers.count(target)) {
            send(onlineUsers[target], (char*)cPkt, sizeof(ChatMessagePacket), 0);
        }
    }
    }

    if (!currentUsername.empty()) {
            BroadcastStatusToFriends(currentUsername, 0);

            std::lock_guard<std::mutex> lock(users_mutex);
            onlineUsers.erase(currentUsername);
            std::cout << "[SERVER] User logout: " << currentUsername << std::endl;
        }
        
        // Стандартная очистка сокета
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            clients.erase(std::remove(clients.begin(), clients.end(), client_socket), clients.end());
        }
        closesocket(client_socket);
    }


int main() {
    SetConsoleCP(65001); SetConsoleOutputCP(65001);
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

    std::cout << "[SERVER] AEGIS Online на порту 5555" << std::endl;

    while (true) {
        SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            clients.push_back(clientSocket);
        }
        std::thread(HandleClient, clientSocket).detach();
    }

    db.Disconnect();
    closesocket(serverSocket);
    WSACleanup();
    return 0;
}