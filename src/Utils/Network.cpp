#include "Network.h"
#include "Utils/Utils.h"
#include "Components/MessageList.h"
#include "Components/MessageInput.h"
#include "Components/SidebarFriends.h"
#include "AuthProtocol.h" 
#include "Pages/FriendsPage.h"
#include <ws2tcpip.h>
#include <vector>
#include <algorithm>
#include <mutex>
#include <map>


SOCKET clientSocket = INVALID_SOCKET;
bool isConnected = false;
std::thread receiveThread;
std::string userName = "User"; 
std::string currentUserName = "";
std::string userAvatar = "[^.^]";
extern std::vector<PendingRequest> pendingRequests;
extern std::mutex pendingMutex; 
extern HWND hMainWnd;
std::map<std::string, std::vector<Message>> chatHistories;
bool ConnectToServer(const std::string& address, const std::string& port) {
    if (clientSocket != INVALID_SOCKET) {
        isConnected = false;
        closesocket(clientSocket);
        clientSocket = INVALID_SOCKET;
    }
    if (receiveThread.joinable()) receiveThread.join();

    struct addrinfo hints, *result = NULL;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    int res = getaddrinfo(address.c_str(), port.c_str(), &hints, &result);
    if (res != 0) return false;
    
    clientSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (clientSocket == INVALID_SOCKET) {
        freeaddrinfo(result);
        return false;
    }

    if (connect(clientSocket, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) {
        closesocket(clientSocket);
        clientSocket = INVALID_SOCKET;
        freeaddrinfo(result);
        return false;
    }

    freeaddrinfo(result);
    isConnected = true;
    return true;
}



void StartMessageSystem() {
    if (isConnected && clientSocket != INVALID_SOCKET) {
        if (!receiveThread.joinable()) {
            receiveThread = std::thread(ReceiveMessages);
        }
    }
}

void ParseMessage(const std::string& msg) {
    Message m;
    m.isMine = false;
    m.isUser = false;
    m.timeStr = GetCurrentTimeStr();
    
    std::string cleanMsg = msg;
    size_t pos = 0;
    while ((pos = cleanMsg.find("\033[")) != std::string::npos) {
        size_t end = cleanMsg.find("m", pos);
        if (end != std::string::npos) cleanMsg.erase(pos, end - pos + 1);
        else break;
    }
        
    size_t colonPos = cleanMsg.find(": ");
    if (colonPos != std::string::npos) {
        std::string prefix = cleanMsg.substr(0, colonPos);
        m.text = cleanMsg.substr(colonPos + 2);

        // Ставим isMine, если сообщение от меня
        m.isMine = (!userName.empty() && prefix.find(userName) != std::string::npos);

        size_t avatarEnd = prefix.find("]");
        if (avatarEnd != std::string::npos && avatarEnd + 1 < prefix.length()) {
            m.sender = prefix.substr(avatarEnd + 1);
            size_t firstChar = m.sender.find_first_not_of(" ");
            if (firstChar != std::string::npos) m.sender.erase(0, firstChar);
        } else {
            m.sender = prefix;
        }
    } else {
        m.text = cleanMsg;
        m.sender = "System";
        m.isMine = false;
    }
    
    messages.push_back(m);
    InvalidateRect(hMessageList, NULL, TRUE);

    RECT rect;
    GetClientRect(hMessageList, &rect);
    int totalHeight = 20;
    for (const auto& msgItem : messages) {
        totalHeight += GetMessageHeight(msgItem, rect.right) + 10;
    }
    
    if (totalHeight > rect.bottom) {
        SCROLLINFO si = {};
        si.cbSize = sizeof(SCROLLINFO);
        si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
        si.nMin = 0;
        si.nMax = totalHeight;
        si.nPage = rect.bottom;
        si.nPos = totalHeight - rect.bottom;
        SetScrollInfo(hMessageList, SB_VERT, &si, TRUE);
        scrollPos = totalHeight - rect.bottom + 20;
    }
}

bool SendPacket(const char* data, int size) {
    if (clientSocket == INVALID_SOCKET || !isConnected) {
        return false;
    }

    int totalSent = 0;
    while (totalSent < size) {
        int sent = send(clientSocket, data + totalSent, size - totalSent, 0);
        if (sent <= 0) return false;
        totalSent += sent;
    }
    return true;
}

bool ReceivePacket(char* data, int size) {
    if (clientSocket == INVALID_SOCKET || !isConnected) {
        return false;
    }

    int totalReceived = 0;
    while (totalReceived < size) {
        int received = recv(clientSocket, data + totalReceived, size - totalReceived, 0);
        if (received <= 0) return false;
        totalReceived += received;
    }
    return true;
}

void ReceiveMessages() {
    char buffer[4096];
    while (isConnected && clientSocket != INVALID_SOCKET) {
        ZeroMemory(buffer, sizeof(buffer));
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        
        if (bytesReceived > 0) {
            uint8_t packetType = (uint8_t)buffer[0];

            // 1. Пакет новой входящей заявки в друзья
            if (packetType == PACKET_FRIEND_REQUEST) {
                if (bytesReceived >= sizeof(FriendPacket)) {
                    FriendPacket* fPkt = (FriendPacket*)buffer;
                    char safeName[33] = {0}; 
                    memcpy(safeName, fPkt->senderUsername, 32);
                    
                    {
                        std::lock_guard<std::mutex> lock(pendingMutex);
                        auto it = std::find_if(pendingRequests.begin(), pendingRequests.end(),
                            [&](const PendingRequest& r) { return r.username == safeName; });
                        
                        if (it == pendingRequests.end()) {
                            pendingRequests.push_back({ safeName });
                        }
                    }
                    if (hMainWnd) InvalidateRect(hMainWnd, NULL, TRUE);
                }
            }
        else if (packetType == PACKET_ROOM_LIST) {
            if (bytesReceived >= sizeof(RoomPacket)) {
                RoomPacket* rPkt = (RoomPacket*)buffer;
                char name[65] = {0}; 
                memcpy(name, rPkt->username, 64);
                
                bool status = (rPkt->onlineStatus == 1); 

                AddUserToDMList(hMainWnd, std::string(name), status);
            }
        }
        else if (packetType == PACKET_USER_STATUS) {
            if (bytesReceived >= sizeof(UserStatusPacket)) {
                UserStatusPacket* sPkt = (UserStatusPacket*)buffer;
                
                char targetName[65] = {0};
                memcpy(targetName, sPkt->username, 64);
                
                bool isOnline = (sPkt->onlineStatus == 1);

                UpdateUserOnlineStatus(std::string(targetName), isOnline);

                if (hMainWnd) {
                    InvalidateRect(hMainWnd, NULL, FALSE);
                }
            }
        }

            else if (packetType == PACKET_CHAT_MESSAGE) {
                if (bytesReceived >= sizeof(ChatMessagePacket)) {
                    ChatMessagePacket* cPkt = (ChatMessagePacket*)buffer;
                    std::string sender = cPkt->senderUsername;

                    // Игнорируем свои сообщения, которые уже локально добавлены
                    if (sender == userName) continue;

                    Message m;
                            m.text = cPkt->content;
                            m.sender = cPkt->senderUsername;
                            m.isMine = (sender == userName);         
                            m.timeStr = GetCurrentTimeStr();

                    chatHistories[m.sender].push_back(m);

                    if (g_uiState.activeChatUser == m.sender) {
                        messages.push_back(m);
                    }

                    InvalidateRect(hMainWnd, NULL, FALSE);
                    if (hMessageList) InvalidateRect(hMessageList, NULL, TRUE);
                }
            }

            // 2. Пакет подтверждения дружбы (ЗАЯВКА ПРИНЯТА)
            else if (packetType == PACKET_FRIEND_ACCEPT) {
                if (bytesReceived >= sizeof(FriendActionPacket)) {
                    FriendActionPacket* aPkt = (FriendActionPacket*)buffer;
                    char safeName[65] = {0}; // Размер из AuthProtocol.h (64 + 1)
                    memcpy(safeName, aPkt->targetUsername, 64);

                    // Добавляем в список ЛС
                    AddUserToDMList(hMainWnd, std::string(safeName));

                    // Если этот человек был в списке "Ожидание", удаляем его оттуда
                    {
                        std::lock_guard<std::mutex> lock(pendingMutex);
                        pendingRequests.erase(
                            std::remove_if(pendingRequests.begin(), pendingRequests.end(),
                                [&](const PendingRequest& r) { return r.username == safeName; }),
                            pendingRequests.end()
                        );
                    }

                    if (hMainWnd) InvalidateRect(hMainWnd, NULL, FALSE);
                }
            }
            // 3. Обычный текстовый чат (для обратной совместимости)
            else {
                buffer[bytesReceived] = '\0';
                ParseMessage(std::string(buffer));
            }
        } else {
            break;
        }
    }
    isConnected = false;
    if (clientSocket != INVALID_SOCKET) {
        closesocket(clientSocket);
        clientSocket = INVALID_SOCKET;
    }
}

void OnFriendRequestAccepted(std::string username) {
    AddUserToDMList(hMainWnd, username);
    InvalidateRect(hMainWnd, NULL, FALSE);
}

void SendPrivateMessage(const std::string& target, const std::string& text) {
    if (text.empty() || target.empty() || !isConnected) return;

    ChatMessagePacket pkt{};
    pkt.type = PACKET_CHAT_MESSAGE;

    strncpy(pkt.senderUsername, userName.c_str(), sizeof(pkt.senderUsername) - 1);
    strncpy(pkt.targetUsername, target.c_str(), sizeof(pkt.targetUsername) - 1);
    strncpy(pkt.content, text.c_str(), sizeof(pkt.content) - 1);


    if (send(clientSocket, (char*)&pkt, sizeof(ChatMessagePacket), 0) != SOCKET_ERROR) {
        Message m;
        m.text = text;
        m.sender = userName; // Моё имя
        m.isMine = true;
        m.time = GetCurrentTimeStr();

        chatHistories[target].push_back(m);

        if (g_uiState.activeChatUser == target) {
            messages.push_back(m);   // ← ДОБАВИТЬ
        }

        InvalidateRect(hMessageList, NULL, TRUE);
    
        if (hMainWnd) {
            InvalidateRect(hMainWnd, NULL, FALSE); 
            UpdateWindow(hMainWnd);
        }
    }
}

void SendPrivateMessageFromUI() {
    if (!isConnected || clientSocket == INVALID_SOCKET) return;
    if (g_uiState.activeChatUser.empty()) return;

    char msgBuf[447]; 
    GetWindowTextA(hInputEdit, msgBuf, 447);
    std::string messageText = msgBuf;
    if (messageText.empty()) return; // Не шлем пустые

    ChatMessagePacket pkt{};
    pkt.type = PACKET_CHAT_MESSAGE;

    // Важно: копируем ваше актуальное имя пользователя
    strncpy(pkt.senderUsername, userName.c_str(), sizeof(pkt.senderUsername) - 1);
    strncpy(pkt.targetUsername, g_uiState.activeChatUser.c_str(), sizeof(pkt.targetUsername) - 1);
    strncpy(pkt.content, messageText.c_str(), sizeof(pkt.content) - 1);

    if (send(clientSocket, (char*)&pkt, sizeof(ChatMessagePacket), 0) != SOCKET_ERROR) { 
        Message m;
        m.text = messageText;
        m.sender = userName;
        m.isMine = true;        // Явно ПРАВОЕ сообщение
        m.timeStr = GetCurrentTimeStr(); // Используем ТОЛЬКО timeStr
        m.time = m.timeStr;              // Для подстраховки заполним оба

        chatHistories[g_uiState.activeChatUser].push_back(m);
        
        // Добавляем в текущий экран, если мы в том же чате
        messages.push_back(m); 

        SetWindowTextA(hInputEdit, "");
        
        // Обновляем оба окна
        InvalidateRect(hMessageList, NULL, TRUE);
        if (hMainWnd) InvalidateRect(hMainWnd, NULL, FALSE);
    }
}