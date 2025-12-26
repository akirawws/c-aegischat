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
extern std::vector<Message> messages;
extern std::string activeChatUser; 
extern std::map<std::string, std::vector<Message>> chatHistories;
extern int g_historyOffset;
bool g_isLoadingHistory = false;
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
                AddUserToDMList(hMainWnd, std::string(name), (rPkt.onlineStatus == 1));
            }
        }
        else if (packetType == PACKET_USER_STATUS) {
            UserStatusPacket sPkt;
            sPkt.type = packetType;
            if (ReceiveExact((char*)&sPkt + 1, sizeof(UserStatusPacket) - 1)) {
                char targetName[65] = {0};
                memcpy(targetName, sPkt.username, 64);
                UpdateUserOnlineStatus(std::string(targetName), (sPkt.onlineStatus == 1));
                if (hMainWnd) InvalidateRect(hMainWnd, NULL, FALSE);
            }
        }
        if (packetType == PACKET_CHAT_MESSAGE) {
            ChatMessagePacket cPkt;
            cPkt.type = packetType;
            if (ReceiveExact((char*)&cPkt + 1, sizeof(ChatMessagePacket) - 1)) {
                std::string sender = cPkt.senderUsername;
                
                Message m;
                m.text = std::string(cPkt.content);
                m.sender = sender;
                m.isMine = (sender == userName); 
                m.timeStr = GetCurrentTimeStr();

                chatHistories[sender].push_back(m);
                if (g_uiState.activeChatUser == sender) {
                    messages.push_back(m);
                    if (hMessageList) {
                        InvalidateRect(hMessageList, NULL, TRUE);
                        // Авто-скролл вниз
                        PostMessage(hMessageList, WM_VSCROLL, SB_BOTTOM, 0);
                    }
                }
                if (hMainWnd) InvalidateRect(hMainWnd, NULL, FALSE);
            }
        }
            else if (packetType == PACKET_CHAT_HISTORY) {
                ChatHistoryEntryPacket hPkt;
                hPkt.type = packetType;
                if (ReceiveExact((char*)&hPkt + 1, sizeof(ChatHistoryEntryPacket) - 1)) {
                    
                    // Используем статический флаг для очистки только перед первым сообщением ПАЧКИ
                    static bool needsClear = true; 

                    if (needsClear) {
                        chatHistories[g_uiState.activeChatUser].clear();
                        messages.clear(); // Сразу очищаем экран
                        needsClear = false;
                    }

                    Message m;
                    m.text = std::string(hPkt.content);
                    m.sender = hPkt.senderUsername;
                    m.timeStr = hPkt.timestamp;
                    m.isMine = (m.sender == userName);

                    // 1. Добавляем в кэш
                    chatHistories[g_uiState.activeChatUser].push_back(m);
                    
                    // 2. СРАЗУ добавляем в активный список, чтобы сообщения появлялись постепенно
                    messages.push_back(m);

                    // Отрисовываем каждое сообщение (опционально для плавности)
                    if (hMessageList) {
                        InvalidateRect(hMessageList, NULL, FALSE);
                    }

                    if (hPkt.isLast) {
                        g_isLoadingHistory = false;
                        needsClear = true; // Сбрасываем флаг для следующего запроса истории
                        
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
            // Если пришел неизвестный пакет или старый текстовый формат
            // В твоем старом коде тут был ParseMessage. 
            // Но в протоколе на структурах сюда попадать не должно.
        }
    }
    // Конец цикла — закрываем всё
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

    // 1. Читаем текст из Edit в WideChar (Unicode), чтобы сохранить русские буквы
    wchar_t wMsgBuf[512]; 
    GetWindowTextW(hInputEdit, wMsgBuf, 512); 
    if (wcslen(wMsgBuf) == 0) return;

    // 2. Конвертируем Unicode (UTF-16) в UTF-8 для отправки по сети
    char utf8Content[sizeof(ChatMessagePacket().content)]; 
    ZeroMemory(utf8Content, sizeof(utf8Content));
    
    WideCharToMultiByte(CP_UTF8, 0, wMsgBuf, -1, utf8Content, sizeof(utf8Content) - 1, NULL, NULL);

    // 3. Формируем пакет
    ChatMessagePacket pkt{};
    pkt.type = PACKET_CHAT_MESSAGE;

    // Очищаем поля перед заполнением
    memset(pkt.senderUsername, 0, 64);
    memset(pkt.targetUsername, 0, 64);

    strncpy(pkt.senderUsername, userName.c_str(), 63);
    strncpy(pkt.targetUsername, g_uiState.activeChatUser.c_str(), 63);
    
    // Копируем уже готовый UTF-8 контент
    memcpy(pkt.content, utf8Content, sizeof(pkt.content));

    // 4. Отправляем через ваш SendPacket (который должен содержать цикл while)
    if (SendPacket((char*)&pkt, sizeof(ChatMessagePacket))) { 
        Message m;
        // Для локального отображения конвертируем обратно или используем строку как есть
        m.text = utf8Content; 
        m.sender = userName;
        m.isMine = true;
        m.timeStr = GetCurrentTimeStr();

        chatHistories[g_uiState.activeChatUser].push_back(m);
        messages.push_back(m); 

        // Очищаем поле ввода (Unicode-версия)
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

    // Сбрасываем глобальный оффсет, чтобы блок ReceiveMessages знал, что пора чистить кэш
    g_historyOffset = offset; 
    g_isLoadingHistory = true;

    HistoryRequestPacket req;
    req.type = PACKET_CHAT_HISTORY;
    req.offset = offset;
    memset(req.targetUsername, 0, 64);
    strncpy(req.targetUsername, target.c_str(), 63);

    SendPacket((char*)&req, sizeof(HistoryRequestPacket));
}