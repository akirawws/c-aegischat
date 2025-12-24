#include "Network.h"
#include "Utils/Utils.h"
#include "Components/MessageList.h"
#include "Components/MessageInput.h"
#include <ws2tcpip.h>
#include <vector>
#include <algorithm>

SOCKET clientSocket = INVALID_SOCKET;
bool isConnected = false;
std::thread receiveThread;
std::string userName = "User"; 
std::string userAvatar = "[^.^]";

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
    receiveThread = std::thread(ReceiveMessages);
    return true;
}

void SendChatMessage() {
    extern HWND hInputEdit;
    if (!isConnected || clientSocket == INVALID_SOCKET) return;

    int len = GetWindowTextLengthW(hInputEdit);
    if (len > 0) {
        std::vector<wchar_t> buffer(len + 1);
        GetWindowTextW(hInputEdit, buffer.data(), len + 1);
        
        std::string text = WideToUtf8(buffer.data());
        std::string fullMsg = userAvatar + " " + userName + ": " + text + "\n";

        if (send(clientSocket, fullMsg.c_str(), (int)fullMsg.length(), 0) != SOCKET_ERROR) {
            SetWindowTextW(hInputEdit, L"");
            UpdateInputHeight(GetParent(hInputEdit), hInputEdit, hMessageList);
        }
    }
}

void ParseMessage(const std::string& msg) {
    Message m;
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
        
        if (!userName.empty() && prefix.find(userName) != std::string::npos) {
            m.isUser = true;
        }
        
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
        m.isUser = false;
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

void ReceiveMessages() {
    char buffer[4096];
    while (isConnected && clientSocket != INVALID_SOCKET) {
        ZeroMemory(buffer, 4096);
        int bytesReceived = recv(clientSocket, buffer, 4050, 0);
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            ParseMessage(std::string(buffer));
        } else {
            break;
        }
    }
    isConnected = false;
}