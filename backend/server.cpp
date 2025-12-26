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
    char buffer[1024]; // Увеличили буфер для безопасности
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

                    std::vector<std::string> pending = db.GetPendingRequests(currentUsername);
                    for (const auto& senderName : pending) {
                        FriendPacket fp = { PACKET_FRIEND_REQUEST };
                        strncpy(fp.senderUsername, senderName.c_str(), 63);
                        strncpy(fp.targetUsername, currentUsername.c_str(), 63);
                        send(client_socket, (char*)&fp, sizeof(FriendPacket), 0);
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
            HistoryRequestPacket req{};
            req.type = packetType;

            // Дочитываем пакет полностью (кроме первого байта type)
            if (!ReceiveExact(
                    client_socket,
                    (char*)&req + 1,
                    sizeof(HistoryRequestPacket) - 1)) {
                break;
            }

            std::cout << "[DEBUG] History request from "
                    << currentUsername
                    << " for " << req.targetUsername
                    << " offset " << req.offset << std::endl;

            std::vector<Message> history =
                db.GetChatHistory(currentUsername, req.targetUsername, req.offset, 50);

            std::cout << "[DEBUG] Found messages: " << history.size() << std::endl;

            // Если истории нет — отправляем пустой пакет с isLast=true
            if (history.empty()) {
                ChatHistoryEntryPacket emptyPkt{};
                emptyPkt.type = PACKET_CHAT_HISTORY;
                emptyPkt.isLast = true;

                strcpy(emptyPkt.senderUsername, "system");
                emptyPkt.content[0] = '\0';
                emptyPkt.timestamp[0] = '\0';

                send(client_socket, (char*)&emptyPkt, sizeof(emptyPkt), 0);
            }
            else {
                // Отправляем от старых к новым
                for (int i = (int)history.size() - 1; i >= 0; --i) {
                    ChatHistoryEntryPacket hPkt{};
                    hPkt.type = PACKET_CHAT_HISTORY;

                    strncpy(hPkt.senderUsername, history[i].sender.c_str(), 63);
                    strncpy(hPkt.content, history[i].text.c_str(), 511);
                    strncpy(hPkt.timestamp, history[i].timeStr.c_str(), 31);

                    hPkt.senderUsername[63] = '\0';
                    hPkt.content[511] = '\0';
                    hPkt.timestamp[31] = '\0';

                    hPkt.isLast = (i == 0);

                    send(client_socket, (char*)&hPkt, sizeof(hPkt), 0);

                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
            }
        }

        else if (packetType == PACKET_CHAT_MESSAGE) {
            ChatMessagePacket cPkt;
            size_t packetSize = sizeof(ChatMessagePacket);
            memcpy(&cPkt, buffer, (bytesReceived > packetSize ? packetSize : bytesReceived));
            
            int totalRead = bytesReceived;
            while (totalRead < (int)packetSize) {
                int extra = recv(client_socket, (char*)&cPkt + totalRead, (int)(packetSize - totalRead), 0);
                if (extra <= 0) break;
                totalRead += extra;
            }

            cPkt.senderUsername[63] = '\0';
            cPkt.targetUsername[63] = '\0';
            cPkt.content[sizeof(cPkt.content) - 1] = '\0';

            if (currentUsername == cPkt.senderUsername) {
                if (db.SaveMessage(cPkt.senderUsername, cPkt.targetUsername, cPkt.content)) {
                    std::lock_guard<std::mutex> lock(users_mutex);
                    if (onlineUsers.count(cPkt.targetUsername)) {
                        send(onlineUsers[cPkt.targetUsername], (char*)&cPkt, sizeof(ChatMessagePacket), 0);
                    }
                }
            }
        }
    } // Конец while(true)

    // Очистка при отключении
    if (!currentUsername.empty()) {
        BroadcastStatusToFriends(currentUsername, 0);
        std::lock_guard<std::mutex> lock(users_mutex);
        onlineUsers.erase(currentUsername);
        std::cout << "[SERVER] User logout: " << currentUsername << std::endl;
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