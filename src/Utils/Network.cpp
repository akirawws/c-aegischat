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
#include <string>
#include <iostream>


std::map<std::string, std::vector<Message>> chatHistories;
std::vector<Message> messages; 
std::vector<DMUser> dmUsers;
std::string activeChatUser = "";
extern std::string userName;

int g_historyOffset = 0;
bool g_isLoadingHistory = false;
SOCKET clientSocket = INVALID_SOCKET;
bool isConnected = false;

std::thread receiveThread;
std::string userName = "User"; 
std::string currentUserName = "";
std::string userAvatar = "[^.^]";
std::mutex dmMutex; 

extern std::vector<PendingRequest> pendingRequests;
extern std::mutex pendingMutex; 
extern HWND hMainWnd;
extern HWND hMessageList;
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

    if (hMessageList) {
        InvalidateRect(hMessageList, NULL, TRUE);
        PostMessage(hMessageList, WM_VSCROLL, SB_BOTTOM, 0);
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
bool ReceiveExact(char* data, int size) {
    int totalReceived = 0;
    while (totalReceived < size) {
        int received = recv(clientSocket, data + totalReceived, size - totalReceived, 0);
        if (received <= 0) return false;
        totalReceived += received;
    }
    return true;
}

void ReceiveMessages() {
    while (isConnected && clientSocket != INVALID_SOCKET) {
        uint8_t packetType;

        int res = recv(clientSocket, (char*)&packetType, 1, 0);
        if (res <= 0) break;

        if (packetType == PACKET_FRIEND_REQUEST) {
            FriendPacket fPkt;
            fPkt.type = packetType;
            if (ReceiveExact((char*)&fPkt + 1, sizeof(FriendPacket) - 1)) {
                char safeName[33] = {0}; 
                memcpy(safeName, fPkt.senderUsername, 32);
                {
                    std::lock_guard<std::mutex> lock(pendingMutex);
                    auto it = std::find_if(pendingRequests.begin(), pendingRequests.end(),
                        [&](const PendingRequest& r) { return r.username == safeName; });
                    if (it == pendingRequests.end()) pendingRequests.push_back({ safeName });
                }
                if (hMainWnd) InvalidateRect(hMainWnd, NULL, TRUE);
            }
        }
            else if (packetType == PACKET_ROOM_LIST) {
                RoomPacket rPkt;
                rPkt.type = packetType;
                if (ReceiveExact((char*)&rPkt + 1, sizeof(RoomPacket) - 1)) {
                    char name[65] = {0}; 
                    memcpy(name, rPkt.username, 64);
                    
                    // Флаг isGroup: если это комната/группа, ставим true
                    bool isGroup = (rPkt.onlineStatus == 2); // Например, статус 2 на сервере значит "группа"
                    
                    AddUserToDMList(hMainWnd, std::string(name), (rPkt.onlineStatus >= 1));
                    
                    if (hMainWnd) InvalidateRect(hMainWnd, NULL, FALSE);
                }
            }
        else if (packetType == PACKET_USER_STATUS) {
            UserStatusPacket sPkt;
            sPkt.type = packetType;
            if (ReceiveExact((char*)&sPkt + 1, sizeof(UserStatusPacket) - 1)) {
                char targetName[65] = {0};
                memcpy(targetName, sPkt.username, 64);
                
                UpdateUserOnlineStatus(std::string(targetName), (sPkt.onlineStatus == 1));
                
                if (hMainWnd) {
                    InvalidateRect(hMainWnd, NULL, FALSE);
                    UpdateWindow(hMainWnd); 
                }
            }
        }

        if (packetType == PACKET_CHAT_MESSAGE) {
            ChatMessagePacket cPkt{};
            cPkt.type = PACKET_CHAT_MESSAGE;

            if (ReceiveExact((char*)&cPkt + 1, sizeof(ChatMessagePacket) - 1)) {
                std::cout << "[DEBUG] Received msg from: " << cPkt.senderUsername << std::endl;

                std::string sender = cPkt.senderUsername;
                std::string target = cPkt.targetUsername;

                Message m;
                m.text = cPkt.content;
                m.sender = sender;
                m.isMine = (sender == userName);
                m.timeStr = GetCurrentTimeStr();

                std::string chatKey = (target == userName) ? sender : target;

                chatHistories[chatKey].push_back(m);

                if (g_uiState.activeChatUser == chatKey) {
                    messages.push_back(m);
                    InvalidateRect(hMessageList, NULL, TRUE);
                    PostMessage(hMessageList, WM_VSCROLL, SB_BOTTOM, 0);
                }

                InvalidateRect(hMainWnd, NULL, FALSE);
            }
        }

        else if (packetType == PACKET_CREATE_GROUP) {
                    CreateGroupPacket gPkt;
                    gPkt.type = PACKET_CREATE_GROUP;
                    
                    if (ReceiveExact((char*)&gPkt + 1, sizeof(CreateGroupPacket) - 1)) {
                        
                        std::string gName = gPkt.groupName;
                        if (gName.empty() || gName.length() < 2) {
                            gName = "";
                            for (int i = 0; i < gPkt.userCount; ++i) {
                                gName += gPkt.members[i];
                                if (i < gPkt.userCount - 1) gName += ", ";
                            }
                        }
                        AddUserToDMList(hMainWnd, gName, true);
                        
                        if (hMainWnd) {
                            InvalidateRect(hMainWnd, NULL, FALSE);
                            UpdateWindow(hMainWnd);
                        }
                    }
                }
            else if (packetType == PACKET_CHAT_HISTORY) {
                ChatHistoryEntryPacket hPkt;
                hPkt.type = packetType;
                if (ReceiveExact((char*)&hPkt + 1, sizeof(ChatHistoryEntryPacket) - 1)) {
                    static bool needsClear = true; 

                    if (needsClear) {
                        chatHistories[g_uiState.activeChatUser].clear();
                        messages.clear(); 
                        needsClear = false;
                    }

                    Message m;
                    m.text = std::string(hPkt.content);
                    m.sender = hPkt.senderUsername;
                    m.timeStr = hPkt.timestamp;
                    m.isMine = (m.sender == userName);
                    chatHistories[g_uiState.activeChatUser].push_back(m);
                    messages.push_back(m);
                    if (hMessageList) {
                        InvalidateRect(hMessageList, NULL, FALSE);
                    }

                    if (hPkt.isLast) {
                        g_isLoadingHistory = false;
                        needsClear = true; 
                        
                        if (hMessageList) {
                            InvalidateRect(hMessageList, NULL, TRUE);
                            UpdateWindow(hMessageList);
                            PostMessage(hMessageList, WM_VSCROLL, SB_BOTTOM, 0);
                        }
                    }
                }
            }
        else if (packetType == PACKET_FRIEND_ACCEPT) {
            FriendActionPacket aPkt;
            aPkt.type = packetType;
            if (ReceiveExact((char*)&aPkt + 1, sizeof(FriendActionPacket) - 1)) {
                char safeName[65] = {0};
                memcpy(safeName, aPkt.targetUsername, 64);
                AddUserToDMList(hMainWnd, std::string(safeName));
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
        else {
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
        m.sender = userName;
        m.isMine = true;
        m.time = GetCurrentTimeStr();

        chatHistories[target].push_back(m);

        if (g_uiState.activeChatUser == target) {
            messages.push_back(m);   
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
    wchar_t wMsgBuf[512]; 
    GetWindowTextW(hInputEdit, wMsgBuf, 512); 
    if (wcslen(wMsgBuf) == 0) return;

    char utf8Content[sizeof(ChatMessagePacket().content)]; 
    ZeroMemory(utf8Content, sizeof(utf8Content));
    
    WideCharToMultiByte(CP_UTF8, 0, wMsgBuf, -1, utf8Content, sizeof(utf8Content) - 1, NULL, NULL);

    ChatMessagePacket pkt{};
    pkt.type = PACKET_CHAT_MESSAGE;

    memset(pkt.senderUsername, 0, 64);
    memset(pkt.targetUsername, 0, 64);

    strncpy(pkt.senderUsername, userName.c_str(), 63);
    strncpy(pkt.targetUsername, g_uiState.activeChatUser.c_str(), 63);
    
    memcpy(pkt.content, utf8Content, sizeof(pkt.content));

    if (SendPacket((char*)&pkt, sizeof(ChatMessagePacket))) { 
        Message m;
        m.text = utf8Content; 
        m.sender = userName;
        m.isMine = true;
        m.timeStr = GetCurrentTimeStr();
        UpdateUserActivity(g_uiState.activeChatUser);
        chatHistories[g_uiState.activeChatUser].push_back(m);
        messages.push_back(m); 

        SetWindowTextW(hInputEdit, L"");
        
        InvalidateRect(hMessageList, NULL, TRUE);
        if (hMainWnd) InvalidateRect(hMainWnd, NULL, FALSE);
    } else {
        isConnected = false;
        MessageBoxA(hMainWnd, "Соединение разорвано!", "Ошибка", MB_OK | MB_ICONERROR);
    }
}
void RequestChatHistory(const std::string& target, int offset) {
    if (!isConnected || target.empty() || g_isLoadingHistory) return;
    g_historyOffset = offset; 
    g_isLoadingHistory = true;

    HistoryRequestPacket req;
    req.type = PACKET_CHAT_HISTORY;
    req.offset = offset;
    memset(req.targetUsername, 0, 64);
    strncpy(req.targetUsername, target.c_str(), 63);

    SendPacket((char*)&req, sizeof(HistoryRequestPacket));
}

void RequestCreateGroup(const std::vector<std::string>& selectedFriends) {
    CreateGroupPacket pkt;
    memset(&pkt, 0, sizeof(pkt));

    pkt.type = PACKET_CREATE_GROUP;

    std::vector<std::string> allMembers;
    allMembers.push_back(userName); 
    for (const auto& friendName : selectedFriends) {
        allMembers.push_back(friendName);
    }

    std::string autoGroupName = "";
    for (size_t i = 0; i < allMembers.size(); ++i) {
        autoGroupName += allMembers[i];
        if (i < allMembers.size() - 1) {
            autoGroupName += ","; 
        }
    }

    if (autoGroupName.length() > 63) {
        autoGroupName = autoGroupName.substr(0, 60) + "...";
    }

    strncpy(pkt.groupName, autoGroupName.c_str(), 63);
    
    pkt.userCount = (int)allMembers.size();
    for (int i = 0; i < pkt.userCount && i < 10; ++i) {
        strncpy(pkt.members[i], allMembers[i].c_str(), 63);
    }

    if (!SendPacket((char*)&pkt, sizeof(pkt))) {
        MessageBoxA(NULL, "Ошибка отправки запроса на создание группы", "Ошибка", MB_OK | MB_ICONERROR);
    } else {
        AddUserToDMList(hMainWnd, autoGroupName, true);
    }
}

void RequestUserRooms() {
    if (!isConnected || clientSocket == INVALID_SOCKET) return;

    // Создаем пустой пакет-запрос (допустим, тип 12 или используем существующий ROOM_LIST как запрос)
    uint8_t type = PACKET_ROOM_LIST; 
    SendPacket((char*)&type, 1); 
    // Примечание: сервер должен быть настроен так, что если он получает 1 байт PACKET_ROOM_LIST, 
    // он отправляет в ответ список всех групп пользователя.
}
