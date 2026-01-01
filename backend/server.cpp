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

#pragma comment(lib, "ws2_32.lib")

std::vector<SOCKET> clients;
std::mutex clients_mutex;
Database db; 

std::map<std::string, SOCKET> onlineUsers; 
std::mutex users_mutex;

bool ReceiveExact(SOCKET s, char* buf, int size) {
    int total = 0;
    while (total < size) {
        int n = recv(s, buf + total, size - total, 0);
        if (n <= 0) return false;
        total += n;
    }
    return true;
}

void BroadcastStatusToFriends(const std::string& username, uint8_t status) {
    std::vector<std::string> friends = db.GetAcceptedFriends(username);
    
    UserStatusPacket sPkt;
    sPkt.type = PACKET_USER_STATUS;
    memset(sPkt.username, 0, 64);
    strncpy(sPkt.username, username.c_str(), 63);
    sPkt.onlineStatus = status;

    std::lock_guard<std::mutex> lock(users_mutex);
    for (const auto& fName : friends) {
        if (onlineUsers.count(fName)) {
            send(onlineUsers[fName], (char*)&sPkt, sizeof(UserStatusPacket), 0);
        }
    }
}

void HandleClient(SOCKET client_socket) {
    char buffer[1024]; 
    std::string currentUsername = ""; 

    while (true) {
        ZeroMemory(buffer, sizeof(buffer));
        int bytesReceived = recv(client_socket, buffer, sizeof(buffer), 0);

        if (bytesReceived <= 0) break;

        uint8_t packetType = (uint8_t)buffer[0];

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

                    BroadcastStatusToFriends(currentUsername, 1);

                    // Рассылка друзей
                    std::vector<std::string> friends = db.GetAcceptedFriends(currentUsername);
                    for (const auto& fName : friends) {
                        RoomPacket rPkt;
                        rPkt.type = PACKET_ROOM_LIST;
                        memset(rPkt.username, 0, 64);
                        strncpy(rPkt.username, fName.c_str(), 63);
                        {
                            std::lock_guard<std::mutex> lock(users_mutex);
                            rPkt.onlineStatus = (onlineUsers.count(fName) > 0) ? 1 : 0;
                        }
                        send(client_socket, (char*)&rPkt, sizeof(RoomPacket), 0);
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }
                    UserProfile profile = db.GetUserProfile(currentUsername); 
                    
                    UserProfilePacket pPkt;
                    pPkt.type = PACKET_USER_PROFILE;
                    memset(pPkt.username, 0, 64);
                    memset(pPkt.display_name, 0, 64);
                    memset(pPkt.avatar_url, 0, 256);

                    strncpy(pPkt.username, currentUsername.c_str(), 63);
                    strncpy(pPkt.display_name, profile.display_name.c_str(), 63);
                    strncpy(pPkt.avatar_url, profile.avatar_url.c_str(), 255);

                    send(client_socket, (char*)&pPkt, sizeof(UserProfilePacket), 0);
                    std::cout << "[SERVER] Профиль отправлен: " << profile.display_name << std::endl;
                    // ------------------------------------

                    BroadcastStatusToFriends(currentUsername, 1);
                    
                    // --- ИНИЦИАЛИЗАЦИЯ ГРУПП ПРИ ВХОДЕ ---
                    std::vector<std::string> userGroups = db.GetUserGroups(currentUsername);
                    for (const auto& gName : userGroups) {
                        CreateGroupPacket gPkt;
                        gPkt.type = PACKET_CREATE_GROUP;
                        memset(gPkt.groupName, 0, 64);
                        strncpy(gPkt.groupName, gName.c_str(), 63);

                        std::vector<std::string> members = db.GetGroupMembers(gName);
                        gPkt.userCount = (int)members.size();
                        for (int i = 0; i < gPkt.userCount && i < 10; ++i) {
                            memset(gPkt.members[i], 0, 64);
                            strncpy(gPkt.members[i], members[i].c_str(), 63);
                        }
                        send(client_socket, (char*)&gPkt, sizeof(CreateGroupPacket), 0);
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }

                    // Заявки в друзья
                    std::vector<std::string> pending = db.GetPendingRequests(currentUsername);
                    for (const auto& senderName : pending) {
                        FriendPacket fp = { PACKET_FRIEND_REQUEST };
                        strncpy(fp.senderUsername, senderName.c_str(), 63);
                        strncpy(fp.targetUsername, currentUsername.c_str(), 63);
                        send(client_socket, (char*)&fp, sizeof(FriendPacket), 0);
                        std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    }
                } else {
                    strcpy(res.message, "Invalid credentials!");
                    send(client_socket, (char*)&res, sizeof(ResponsePacket), 0);
                }
            }
        } 
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
            if (db.AcceptFriendAndCreateRoom(aPkt->targetUsername, currentUsername)) {
                std::lock_guard<std::mutex> lock(users_mutex);
                if (onlineUsers.count(aPkt->targetUsername)) {
                    FriendActionPacket notification = { PACKET_FRIEND_ACCEPT };
                    strncpy(notification.targetUsername, currentUsername.c_str(), 63);
                    send(onlineUsers[aPkt->targetUsername], (char*)&notification, sizeof(FriendActionPacket), 0);
                }
                FriendActionPacket confirm = { PACKET_FRIEND_ACCEPT };
                strncpy(confirm.targetUsername, aPkt->targetUsername, 63);
                send(client_socket, (char*)&confirm, sizeof(FriendActionPacket), 0);
            }
        }
        else if (packetType == PACKET_CHAT_HISTORY) {
            HistoryRequestPacket* req = (HistoryRequestPacket*)buffer;
            std::vector<Message> history = db.GetChatHistory(currentUsername, req->targetUsername, req->offset, 50);

            if (history.empty()) {
                ChatHistoryEntryPacket emptyPkt = { PACKET_CHAT_HISTORY };
                emptyPkt.isLast = true;
                send(client_socket, (char*)&emptyPkt, sizeof(ChatHistoryEntryPacket), 0);
            } else {
                for (int i = (int)history.size() - 1; i >= 0; --i) {
                    ChatHistoryEntryPacket hPkt = { PACKET_CHAT_HISTORY };
                    strncpy(hPkt.senderUsername, history[i].sender.c_str(), 63);
                    strncpy(hPkt.content, history[i].text.c_str(), 511);
                    strncpy(hPkt.timestamp, history[i].timeStr.c_str(), 31);
                    hPkt.isLast = (i == 0); 
                    send(client_socket, (char*)&hPkt, sizeof(ChatHistoryEntryPacket), 0);
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
            }
        }
        else if (packetType == PACKET_CREATE_GROUP) {
            CreateGroupPacket* gPkt = (CreateGroupPacket*)buffer;
            std::vector<std::string> memberList;
            memberList.push_back(currentUsername);
            for (int i = 0; i < gPkt->userCount; i++) {
                memberList.push_back(gPkt->members[i]);
            }

            if (db.CreateGroup(gPkt->groupName, memberList)) {
                std::lock_guard<std::mutex> lock(users_mutex);
                for (const auto& member : memberList) {
                    if (onlineUsers.count(member)) {
                        send(onlineUsers[member], (char*)gPkt, sizeof(CreateGroupPacket), 0);
                    }
                }
            }
        }
    else if (packetType == PACKET_CHAT_MESSAGE) {
        std::cout << "\n[SERVER] ===== ПОЛУЧЕН PACKET_CHAT_MESSAGE =====" << std::endl;
        
        // Данные УЖЕ находятся в buffer, так как мы сделали recv в начале цикла.
        // Накладываем структуру на буфер:
        ChatMessagePacket* p = (ChatMessagePacket*)buffer;

        char sender[64] = {0};
        char target[64] = {0};
        char content[512] = {0};
        
        // Копируем данные из структуры
        strncpy(sender, p->senderUsername, 63);
        strncpy(target, p->targetUsername, 63);
        strncpy(content, p->content, 511);
        
        std::cout << "[SERVER] Пакет извлечен из буфера:" << std::endl;
        std::cout << "[SERVER] Отправитель: '" << sender << "'" << std::endl;
        std::cout << "[SERVER] Получатель: '" << target << "'" << std::endl;
        std::cout << "[SERVER] Содержимое: '" << content << "'" << std::endl;
    
    if (currentUsername == std::string(sender)) {
        std::cout << "[SERVER] Проверка отправителя: OK (текущий пользователь совпадает с отправителем)" << std::endl;
        
        // Проверяем, группа ли это
        bool isGroup = db.IsGroup(target);
        std::cout << "[SERVER] Это группа? " << (isGroup ? "ДА" : "НЕТ") << std::endl;
        
        if (db.SaveMessage(sender, target, content)) {
            std::cout << "[SERVER] Сообщение сохранено в БД" << std::endl;
            
            if (isGroup) {
                std::vector<std::string> members = db.GetGroupMembers(target);
                std::cout << "[SERVER] Члены группы (" << members.size() << "): ";
                for (const auto& m : members) std::cout << m << " ";
                std::cout << std::endl;
                
                // Создаем полный пакет для отправки другим участникам
                ChatMessagePacket fullPkt;
                fullPkt.type = PACKET_CHAT_MESSAGE;
                strncpy(fullPkt.senderUsername, sender, 63);
                strncpy(fullPkt.targetUsername, target, 63);
                strncpy(fullPkt.content, content, 511);
                
                std::lock_guard<std::mutex> lock(users_mutex);
                for (const auto& m : members) {
                    if (m != currentUsername && onlineUsers.count(m)) {
                        send(onlineUsers[m], (char*)&fullPkt, sizeof(ChatMessagePacket), 0);
                        std::cout << "[SERVER] Отправлено участнику: " << m << std::endl;
                    }
                }
            } else {
                // Создаем полный пакет для отправки получателю
                ChatMessagePacket fullPkt;
                fullPkt.type = PACKET_CHAT_MESSAGE;
                strncpy(fullPkt.senderUsername, sender, 63);
                strncpy(fullPkt.targetUsername, target, 63);
                strncpy(fullPkt.content, content, 511);
                
                std::lock_guard<std::mutex> lock(users_mutex);
                if (onlineUsers.count(target)) {
                    send(onlineUsers[target], (char*)&fullPkt, sizeof(ChatMessagePacket), 0);
                    std::cout << "[SERVER] Отправлено пользователю: " << target << std::endl;
                }
            }
        } else {
            std::cout << "[SERVER ERROR] Не удалось сохранить сообщение в БД!" << std::endl;
        }
    } else {
        std::cout << "[SERVER WARNING] Отправитель не совпадает с текущим пользователем!" << std::endl;
        std::cout << "[SERVER WARNING] currentUsername: '" << currentUsername << "', sender: '" << sender << "'" << std::endl;
    }
    
    std::cout << "[SERVER] ===== ОБРАБОТКА PACKET_CHAT_MESSAGE ЗАВЕРШЕНА =====\n" << std::endl;
}
}

    if (!currentUsername.empty()) {
        BroadcastStatusToFriends(currentUsername, 0);
        std::lock_guard<std::mutex> lock(users_mutex);
        onlineUsers.erase(currentUsername);
    }
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        clients.erase(std::remove(clients.begin(), clients.end(), client_socket), clients.end());
    }
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

    std::cout << "[SERVER] AEGIS Online на порту 5555" << std::endl;

    while (true) {
        SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);
        if (clientSocket != INVALID_SOCKET) {
            {
                std::lock_guard<std::mutex> lock(clients_mutex);
                clients.push_back(clientSocket);
            }
            std::thread(HandleClient, clientSocket).detach();
        }
    }

    db.Disconnect();
    closesocket(serverSocket);
    WSACleanup();
    return 0;
}