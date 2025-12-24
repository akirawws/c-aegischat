#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <string>
#include <windows.h>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

// ANSI цвета
#define CLR_BG    "\033[44m"
#define CLR_RESET "\033[0m"
#define CLR_CYAN  "\033[36m"
#define CLR_GREEN "\033[32m"
#define CLR_GRAY  "\033[90m"
#define CLR_BOLD  "\033[1m"

// Генерация аватарки на основе никнейма
std::string GetAvatar(std::string name) {
    if (name.empty()) return "[?]";
    int hash = 0;
    for (char c : name) hash += c;
    std::string avatars[] = { "[^.^]", "[o_o]", "[x_x]", "[9_9]", "[@_@]", "[*_*]" };
    return avatars[hash % 6];
}

void SetCursor(int x, int y) {
    COORD coord = { (SHORT)x, (SHORT)y };
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
}

void DrawBox(int x, int y, int w, int h, std::string title) {
    SetCursor(x, y);
    std::cout << "┌" << std::string(w - 2, '─') << "┐";
    for (int i = 1; i < h; i++) {
        SetCursor(x, y + i);
        std::cout << "│" << std::string(w - 2, ' ') << "│";
    }
    SetCursor(x, y + h);
    std::cout << "└" << std::string(w - 2, '─') << "┘";
    SetCursor(x + 2, y);
    std::cout << " " << title << " ";
}

void ReceiveMessages(SOCKET s) {
    char buffer[4096];
    while (true) {
        ZeroMemory(buffer, 4096);
        int bytesReceived = recv(s, buffer, 4096, 0);
        if (bytesReceived > 0) {
            // Сохраняем позицию курсора, печатаем сообщение, возвращаем курсор
            std::cout << "\r\033[S\033[1A" << buffer << "\n\n"; 
            std::cout << CLR_GREEN << " > " << CLR_RESET << std::flush;
        } else {
            break;
        }
    }
}

int main() {
    // Включаем поддержку ANSI и UTF-8
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    SetConsoleCP(65001);
    SetConsoleOutputCP(65001);

    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    // --- ИНТЕРФЕЙС ВХОДА ---
    system("cls");
    DrawBox(10, 5, 50, 10, " AEGIS CONNECT ");
    
    std::string address, port, name;
    SetCursor(15, 7);  std::cout << "Server IP  : "; std::cin >> address;
    SetCursor(15, 8);  std::cout << "Server Port: "; std::cin >> port;
    SetCursor(15, 9);  std::cout << "Nickname   : "; std::cin >> name;
    
    SetCursor(15, 12); std::cout << CLR_GRAY << "Подключение..." << CLR_RESET;

    // Настройка сети
    struct addrinfo hints, *result = NULL;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(address.c_str(), port.c_str(), &hints, &result) != 0) {
        std::cout << "\n Ошибка адреса!"; return 1;
    }

    SOCKET clientSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (connect(clientSocket, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) {
        system("cls");
        std::cout << "Не удалось подключиться к " << address << ":" << port << std::endl;
        return 1;
    }
    freeaddrinfo(result);

    // --- ИНТЕРФЕЙС ЧАТА ---
    system("cls");
    std::string avatar = GetAvatar(name);
    std::cout << CLR_BG << " AEGIS CHAT | User: " << name << " " << avatar << " | " << address << CLR_RESET << "\n\n";
    
    std::thread(ReceiveMessages, clientSocket).detach();

    while (true) {
        std::string msg;
        SetCursor(0, 25); // Держим строку ввода внизу
        std::cout << CLR_GREEN << " > " << CLR_RESET;
        std::getline(std::cin >> std::ws, msg);
        
        if (msg == "/quit") break;

        // Визуал сообщения: Аватарка + Имя + Текст
        std::string full_msg = CLR_CYAN + avatar + " " + name + CLR_RESET + ": " + msg;
        
        // Очистка строки ввода и поднятие текста
        std::cout << "\033[A\r\033[K"; 
        send(clientSocket, full_msg.c_str(), (int)full_msg.size() + 1, 0);
    }

    closesocket(clientSocket);
    WSACleanup();
    return 0;
}