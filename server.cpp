#include <iostream>
#include <vector>
#include <string>
#include <winsock2.h>
#include <thread>
#include <mutex>
#include <algorithm>

#pragma comment(lib, "ws2_32.lib")

std::vector<SOCKET> clients;
std::mutex clients_mutex;

void Broadcast(const std::string& message, SOCKET sender_socket) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (auto it = clients.begin(); it != clients.end(); ) {
        // Отправляем всем, включая отправителя, чтобы он видел эхо своего сообщения
        int res = send(*it, message.c_str(), (int)message.size() + 1, 0);
        if (res == SOCKET_ERROR) {
            std::cout << "[!] Ошибка отправки. Удаляем клиента." << std::endl;
            closesocket(*it);
            it = clients.erase(it);
        } else {
            ++it;
        }
    }
}

void HandleClient(SOCKET client_socket) {
    char buffer[4096];
    while (true) {
        ZeroMemory(buffer, 4096);
        int bytesReceived = recv(client_socket, buffer, 4096, 0);

        if (bytesReceived <= 0) {
            std::cout << "[!] Клиент отключился или произошла ошибка." << std::endl;
            break;
        }

        std::string msg(buffer);
        std::cout << "[LOG] Получено: " << msg << std::endl;
        Broadcast(msg, client_socket);
    }

    std::lock_guard<std::mutex> lock(clients_mutex);
    clients.erase(std::remove(clients.begin(), clients.end(), client_socket), clients.end());
    closesocket(client_socket);
}

int main() {
    SetConsoleCP(65001);
    SetConsoleOutputCP(65001);

    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    // ВАЖНО: Используйте порт 5555, как в команде SSH
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    
    sockaddr_in hint;
    hint.sin_family = AF_INET;
    hint.sin_port = htons(5555); // ПОРТ ДОЛЖЕН СОВПАДАТЬ С SSH
    hint.sin_addr.S_un.S_addr = INADDR_ANY;

    bind(serverSocket, (sockaddr*)&hint, sizeof(hint));
    listen(serverSocket, SOMAXCONN);

    std::cout << "[SERVER] Слушаю порт 5555..." << std::endl;

    while (true) {
        SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);
        std::cout << "[+] Новое подключение!" << std::endl;
        
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            clients.push_back(clientSocket);
        }

        std::thread(HandleClient, clientSocket).detach();
    }

    WSACleanup();
    return 0;
}