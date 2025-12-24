#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <string>

#pragma comment(lib, "ws2_32.lib")

// Функция для приема сообщений
void ReceiveMessages(SOCKET s) {
    char buffer[4096];
    while (true) {
        ZeroMemory(buffer, 4096);
        int bytesReceived = recv(s, buffer, 4096, 0);
        if (bytesReceived > 0) {
            std::cout << "\r" << buffer << "\nYou: " << std::flush;
        } else {
            std::cout << "\n[!] Соединение с сервером разорвано.\n";
            break;
        }
    }
}

int main() {
    // Инициализация Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return 1;
    }

    std::string address, port, name;
    std::cout << "Введите адрес (например, a.pinggy.io): ";
    std::cin >> address;
    std::cout << "Введите порт (например, 35317): ";
    std::cin >> port;
    std::cin.ignore();
    std::cout << "Введите ваш никнейм: ";
    std::getline(std::cin, name);

    // Настройка адреса через getaddrinfo (поддерживает и IP, и домены)
    struct addrinfo hints, *result = NULL;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(address.c_str(), port.c_str(), &hints, &result) != 0) {
        std::cerr << "Ошибка: Не удалось распознать адрес." << std::endl;
        WSACleanup();
        return 1;
    }

    // Создание сокета
    SOCKET clientSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Ошибка создания сокета." << std::endl;
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    // Подключение
    std::cout << "Подключение к " << address << ":" << port << "..." << std::endl;
    if (connect(clientSocket, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) {
        std::cerr << "Ошибка подключения. Проверьте, запущен ли сервер и туннель." << std::endl;
        closesocket(clientSocket);
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(result);
    std::cout << "Успешно подключено!\n" << std::endl;

    // Поток для чтения
    std::thread(ReceiveMessages, clientSocket).detach();

    // Основной цикл отправки
    while (true) {
        std::string msg;
        std::cout << "You: ";
        std::getline(std::cin, msg);
        if (msg == "/quit") break;

        std::string full_msg = name + ": " + msg;
        send(clientSocket, full_msg.c_str(), (int)full_msg.size() + 1, 0);
    }

    closesocket(clientSocket);
    WSACleanup();
    return 0;
}