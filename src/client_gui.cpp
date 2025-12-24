#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <commctrl.h>
#include <thread>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cstring>
#ifndef NOMINMAX
#define NOMINMAX
#endif

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "comctl32.lib")

// Цвета темы
#define COLOR_BG_DARK RGB(18, 18, 24)
#define COLOR_BG_SIDEBAR RGB(25, 25, 35)
#define COLOR_BG_BLUE RGB(30, 50, 80)
#define COLOR_BG_BLUE_HOVER RGB(40, 65, 100)
#define COLOR_TEXT_WHITE RGB(240, 240, 245)
#define COLOR_TEXT_GRAY RGB(180, 180, 190)
#define COLOR_ACCENT_BLUE RGB(70, 130, 200)
#define COLOR_ACCENT_BLUE_DARK RGB(50, 100, 160)
#define COLOR_MESSAGE_OTHER RGB(35, 45, 60)
#define COLOR_MESSAGE_USER RGB(50, 90, 140)
int scrollPos = 0;
// Структура сообщения
struct Message {
    std::string text;
    bool isUser;
    std::string sender;
    std::string timeStr; // Новое поле
};

// Глобальные переменные
HWND hMainWnd = NULL;
HWND hConnectWnd = NULL;
HWND hIPEdit = NULL;
HWND hPortEdit = NULL;
HWND hNameEdit = NULL;
HWND hConnectBtn = NULL;
HWND hChatList = NULL;
HWND hMessageList = NULL;
HWND hInputEdit = NULL;
HWND hSendBtn = NULL;
HWND hSidebar = NULL;

SOCKET clientSocket = INVALID_SOCKET;
std::string userName;
std::string userAvatar;
std::vector<Message> messages;
bool isConnected = false;
std::thread receiveThread;

// Генерация аватарки
std::string GetAvatar(const std::string& name) {
    if (name.empty()) return "[?]";
    int hash = 0;
    for (char c : name) hash += c;
    std::string avatars[] = { "[^.^]", "[o_o]", "[x_x]", "[9_9]", "[@_@]", "[*_*]" };
    return avatars[hash % 6];
}
std::string GetCurrentTimeStr() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    char buf[10];
    sprintf(buf, "%02d:%02d", st.wHour, st.wMinute);
    return std::string(buf);
}
// Вычисление высоты сообщения (объявление перед использованием)
int GetMessageHeight(const Message& msg, int width) {
    HDC hdc = GetDC(hMessageList);
    int msgWidth = (int)(width * 0.65);
    RECT textRect = { 0, 0, msgWidth - 24, 0 };
    
    HFONT hFont = CreateFontA(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    
    // Расчет размеров текста
    DrawTextA(hdc, msg.text.c_str(), -1, &textRect, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_CALCRECT);
    
    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
    ReleaseDC(hMessageList, hdc);

    // Явное приведение обоих аргументов к int
    return std::max((int)60, (int)(textRect.bottom + 40));
}


void WriteLog(const std::string& text) {
    // Используем полный путь, чтобы файл создавался рядом с EXE
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    std::string strPath = path;
    strPath = strPath.substr(0, strPath.find_last_of("\\/")) + "\\debug_log.txt";

    FILE* f = fopen(strPath.c_str(), "a");
    if (f) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        fprintf(f, "[%02d:%02d:%02d] %s\n", st.wHour, st.wMinute, st.wSecond, text.c_str());
        fflush(f); // Принудительный сброс данных на диск
        fclose(f);
    }
}

// Парсинг сообщения от сервера
void ParseMessage(const std::string& msg) {
    Message m;
    m.isUser = false;
    m.timeStr = GetCurrentTimeStr(); 
    
    // Удаляем ANSI коды
    std::string cleanMsg = msg;
    size_t pos = 0;
    while ((pos = cleanMsg.find("\033[")) != std::string::npos) {
        size_t end = cleanMsg.find("m", pos);
        if (end != std::string::npos) {
            cleanMsg.erase(pos, end - pos + 1);
        } else {
            break;
        }
    }
    
    // Проверяем, содержит ли сообщение имя пользователя
    size_t colonPos = cleanMsg.find(": ");
    if (colonPos != std::string::npos) {
        std::string prefix = cleanMsg.substr(0, colonPos);
        m.text = cleanMsg.substr(colonPos + 2);
        
        // Проверяем, это ли наше сообщение
        if (prefix.find(userName) != std::string::npos) {
            m.isUser = true;
        }
        
        // Извлекаем имя отправителя (убираем аватарку)
        size_t avatarEnd = prefix.find("]");
        if (avatarEnd != std::string::npos && avatarEnd + 1 < prefix.length()) {
            m.sender = prefix.substr(avatarEnd + 2);
            // Убираем лишние пробелы
            while (!m.sender.empty() && m.sender[0] == ' ') {
                m.sender.erase(0, 1);
            }
        } else {
            m.sender = prefix;
        }
    } else {
        m.text = cleanMsg;
        m.sender = "System";
    }
    
    messages.push_back(m);
    InvalidateRect(hMessageList, NULL, TRUE);
    
    // Обновление окна
    InvalidateRect(hMessageList, NULL, TRUE);
    
    // Прокрутка вниз (автоматически при новом сообщении)
    RECT rect;
    GetClientRect(hMessageList, &rect);
    int totalHeight = 20;
    for (const auto& msg : messages) {
        totalHeight += GetMessageHeight(msg, rect.right) + 10;
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
    WriteLog("Receive thread started.");
    char buffer[4096];
    while (isConnected && clientSocket != INVALID_SOCKET) {
        ZeroMemory(buffer, 4096);
        int bytesReceived = recv(clientSocket, buffer, 4050, 0); // Оставляем место для нуля
        
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            std::string msg(buffer);
            ParseMessage(msg);
        } else if (bytesReceived == 0) {
            WriteLog("Server closed connection.");
            break;
        } else {
            int err = WSAGetLastError();
            if (isConnected) WriteLog("Recv error: " + std::to_string(err));
            break;
        }
    }
    WriteLog("Receive thread exiting.");
    isConnected = false;
}


void DrawMessage(HDC hdc, const Message& msg, int y, int width) {
    // 1. Подготовка текста (конвертация UTF-8 -> UTF-16 для вывода)
    int wLen = MultiByteToWideChar(CP_UTF8, 0, msg.text.c_str(), -1, NULL, 0);
    std::wstring wText(wLen, 0);
    MultiByteToWideChar(CP_UTF8, 0, msg.text.c_str(), -1, &wText[0], wLen);

    int sLen = MultiByteToWideChar(CP_UTF8, 0, msg.sender.c_str(), -1, NULL, 0);
    std::wstring wSender(sLen, 0);
    MultiByteToWideChar(CP_UTF8, 0, msg.sender.c_str(), -1, &wSender[0], sLen);

    // Подготовка времени (Unicode)
    int tLen = MultiByteToWideChar(CP_UTF8, 0, msg.timeStr.c_str(), -1, NULL, 0);
    std::wstring wTime(tLen, 0);
    MultiByteToWideChar(CP_UTF8, 0, msg.timeStr.c_str(), -1, &wTime[0], tLen);

    // 2. Расчет размеров бабла
    int maxBubbleWidth = (int)(width * 0.75);
    
    HFONT hFontMsg = CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, 
                                 RUSSIAN_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, 
                                 CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    
    HDC hdcMeasure = CreateCompatibleDC(hdc);
    SelectObject(hdcMeasure, hFontMsg);
    
    RECT calcRect = { 0, 0, maxBubbleWidth - 30, 0 };
    DrawTextW(hdcMeasure, wText.c_str(), -1, &calcRect, DT_CALCRECT | DT_WORDBREAK);
    DeleteDC(hdcMeasure);

    int finalBubbleWidth = std::max((int)140, (int)calcRect.right + 30);
    int finalHeight = calcRect.bottom + 45; 
    
    int x = msg.isUser ? (width - finalBubbleWidth - 25) : 25;

    // 3. Отрисовка "Бабла"
    SelectObject(hdc, GetStockObject(NULL_PEN));
    HBRUSH bubbleBrush = CreateSolidBrush(msg.isUser ? COLOR_MESSAGE_USER : COLOR_MESSAGE_OTHER);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, bubbleBrush);
    
    RoundRect(hdc, x, y, x + finalBubbleWidth, y + finalHeight, 15, 15);

    // 4. Отрисовка ника и времени
    SetBkMode(hdc, TRANSPARENT);
    
    // Шрифт ника
    HFONT hFontName = CreateFontW(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, 
                                  RUSSIAN_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, 
                                  CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    SelectObject(hdc, hFontName);
    SetTextColor(hdc, msg.isUser ? RGB(160, 200, 255) : COLOR_ACCENT_BLUE);
    
    // Рисуем ник
    TextOutW(hdc, x + 15, y + 8, wSender.c_str(), (int)wcslen(wSender.c_str()));

    // Измеряем ширину ника, чтобы поставить время рядом
    SIZE nameSize;
    GetTextExtentPoint32W(hdc, wSender.c_str(), (int)wcslen(wSender.c_str()), &nameSize);

    // Шрифт времени (чуть меньше и серый)
    HFONT hFontTime = CreateFontW(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, 
                                  RUSSIAN_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, 
                                  CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    SelectObject(hdc, hFontTime);
    SetTextColor(hdc, COLOR_TEXT_GRAY);
    
    // Рисуем время справа от ника (+8 пикселей зазор)
    TextOutW(hdc, x + 15 + nameSize.cx + 8, y + 10, wTime.c_str(), (int)wcslen(wTime.c_str()));

    // 5. Сообщение
    SelectObject(hdc, hFontMsg);
    SetTextColor(hdc, COLOR_TEXT_WHITE);
    RECT textRect = { x + 15, y + 28, x + finalBubbleWidth - 15, y + finalHeight - 10 };
    DrawTextW(hdc, wText.c_str(), -1, &textRect, DT_LEFT | DT_WORDBREAK);

    // Чистим ресурсы
    SelectObject(hdc, oldBrush);
    DeleteObject(bubbleBrush);
    DeleteObject(hFontMsg);
    DeleteObject(hFontName);
    DeleteObject(hFontTime);
}

void OnPaintMessageList(HDC hdc, int width, int height) {
    RECT rect = { 0, 0, width, height };
    HBRUSH brush = CreateSolidBrush(COLOR_BG_DARK);
    FillRect(hdc, &rect, brush);
    DeleteObject(brush);
    
    int y = 20;
    for (const auto& msg : messages) {
        if (y + 60 > 0 && y < height) { 
            DrawMessage(hdc, msg, y, width);
        }
        y += GetMessageHeight(msg, width) + 10;
    }
}

void OnPaintSidebar(HDC hdc, int width, int height) {
    RECT rect;
    GetClientRect(hSidebar, &rect);
    
    HBRUSH brush = CreateSolidBrush(COLOR_BG_SIDEBAR);
    FillRect(hdc, &rect, brush);
    DeleteObject(brush);
    
    SetTextColor(hdc, COLOR_TEXT_WHITE);
    SetBkMode(hdc, TRANSPARENT);
    HFONT hFont = CreateFontA(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    
    RECT textRect = { 20, 20, width - 20, 50 };
    DrawTextA(hdc, "Chats", -1, &textRect, DT_LEFT | DT_VCENTER);
    
    RECT chatRect = { 10, 60, width - 10, 100 };
    brush = CreateSolidBrush(COLOR_BG_BLUE);
    FillRect(hdc, &chatRect, brush);
    DeleteObject(brush);
    
    SetTextColor(hdc, COLOR_TEXT_WHITE);
    SelectObject(hdc, hOldFont);
    hFont = CreateFontA(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    SelectObject(hdc, hFont);
    
    textRect = { 20, 70, width - 20, 90 };
    DrawTextA(hdc, "General Chat", -1, &textRect, DT_LEFT | DT_VCENTER);
    
    DeleteObject(hFont);
}

bool ConnectToServer(const std::string& address, const std::string& port) {
    WriteLog("Step 1: Cleaning up old sockets...");
    if (clientSocket != INVALID_SOCKET) {
        isConnected = false;
        closesocket(clientSocket);
        clientSocket = INVALID_SOCKET;
    }
    if (receiveThread.joinable()) receiveThread.join();

    WriteLog("Step 2: Resolving address: " + address);
    struct addrinfo hints, *result = NULL;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    int res = getaddrinfo(address.c_str(), port.c_str(), &hints, &result);
    if (res != 0) {
        WriteLog("getaddrinfo failed: " + std::to_string(res) + " (" + std::string(gai_strerrorA(res)) + ")");
        return false;
    }
    
    WriteLog("Step 3: Creating socket...");
    clientSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (clientSocket == INVALID_SOCKET) {
        WriteLog("Socket creation failed: " + std::to_string(WSAGetLastError()));
        freeaddrinfo(result);
        return false;
    }

    WriteLog("Step 4: Connecting to server...");
    if (connect(clientSocket, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) {
        int err = WSAGetLastError();
        WriteLog("Connect function failed: " + std::to_string(err));
        closesocket(clientSocket);
        clientSocket = INVALID_SOCKET;
        freeaddrinfo(result);
        return false;
    }

    freeaddrinfo(result);
    WriteLog("Step 5: Connection established successfully.");
    isConnected = true;
    receiveThread = std::thread(ReceiveMessages);
    return true;
}



void SendChatMessage() {
    wchar_t buffer[1024];
    GetWindowTextW(hInputEdit, buffer, 1024); 
    if (wcslen(buffer) == 0) return;

    int size_needed = WideCharToMultiByte(CP_UTF8, 0, buffer, -1, NULL, 0, NULL, NULL);
    std::string utf8_msg(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, buffer, -1, &utf8_msg[0], size_needed, NULL, NULL);

    std::string full_msg = userAvatar + " " + userName + ": " + utf8_msg;
    send(clientSocket, full_msg.c_str(), (int)full_msg.size(), 0);
    SetWindowTextW(hInputEdit, L"");
}


LRESULT CALLBACK ConnectWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {

        hIPEdit = CreateWindowA("EDIT", "xisyrurdm.localto.net",
            WS_VISIBLE | WS_CHILD | ES_LEFT | WS_BORDER,
            50, 80, 300, 30, hwnd, NULL, NULL, NULL);
        
        hPortEdit = CreateWindowA("EDIT", "6162",
            WS_VISIBLE | WS_CHILD | ES_LEFT | WS_BORDER,
            50, 130, 300, 30, hwnd, NULL, NULL, NULL);
        
        hNameEdit = CreateWindowA("EDIT", "",
            WS_VISIBLE | WS_CHILD | ES_LEFT | WS_BORDER,
            50, 180, 300, 30, hwnd, NULL, NULL, NULL);
        
        hConnectBtn = CreateWindowA("BUTTON", "Connect",
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            50, 240, 300, 40, hwnd, (HMENU)1, NULL, NULL);
        

        SendMessageA(hIPEdit, EM_SETCUEBANNER, TRUE, (LPARAM)"Server IP address");
        SendMessageA(hPortEdit, EM_SETCUEBANNER, TRUE, (LPARAM)"Port");
        SendMessageA(hNameEdit, EM_SETCUEBANNER, TRUE, (LPARAM)"Your name");
        break;
    }
case WM_COMMAND:
        if (LOWORD(wParam) == 1) { // Кнопка Connect
            char ip[256], port[256], name[256];
            GetWindowTextA(hIPEdit, ip, 256);
            GetWindowTextA(hPortEdit, port, 256);
            GetWindowTextA(hNameEdit, name, 256);
            
            if (strlen(ip) == 0 || strlen(port) == 0 || strlen(name) == 0) {
                MessageBoxW(hwnd, L"Заполните все поля", L"Ошибка", MB_OK | MB_ICONERROR);
                break;
            }
            
            userName = name;
            userAvatar = GetAvatar(userName);
            
            // Записываем в лог попытку старта
            WriteLog("Connect button clicked. Target: " + std::string(ip) + ":" + std::string(port));

            if (ConnectToServer(ip, port)) {
                WriteLog("Transition to MainWindow...");
                ShowWindow(hwnd, SW_HIDE);
                ShowWindow(hMainWnd, SW_SHOW);
            } else {
                // Если не подключились, получаем код ошибки Winsock
                int lastError = WSAGetLastError();
                std::string logErr = "Connection failed. Winsock Error: " + std::to_string(lastError);
                WriteLog(logErr);
                
                std::wstring wError = L"Ошибка подключения! Код: " + std::to_wstring(lastError) + 
                                      L"\nПроверьте адрес сервера или брандмауэр.";
                MessageBoxW(hwnd, wError.c_str(), L"AEGIS Connection Error", MB_OK | MB_ICONERROR);
            }
        }
        break;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        
        RECT rect;
        GetClientRect(hwnd, &rect);
        HBRUSH brush = CreateSolidBrush(COLOR_BG_DARK);
        FillRect(hdc, &rect, brush);
        DeleteObject(brush);
        
        SetTextColor(hdc, COLOR_TEXT_WHITE);
        SetBkMode(hdc, TRANSPARENT);
        HFONT hFont = CreateFontA(28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
        HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
        
        RECT textRect = { 0, 20, rect.right, 60 };
        DrawTextA(hdc, "AEGIS", -1, &textRect, DT_CENTER | DT_VCENTER);
        
        SelectObject(hdc, hOldFont);
        DeleteObject(hFont);
        

        hFont = CreateFontA(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
        SelectObject(hdc, hFont);
        SetTextColor(hdc, COLOR_TEXT_GRAY);
        
        textRect = { 50, 55, 350, 80 };
        DrawTextA(hdc, "Server IP address", -1, &textRect, DT_LEFT | DT_VCENTER);
        
        textRect = { 50, 105, 350, 130 };
        DrawTextA(hdc, "Port", -1, &textRect, DT_LEFT | DT_VCENTER);
        
        textRect = { 50, 155, 350, 180 };
        DrawTextA(hdc, "Your name", -1, &textRect, DT_LEFT | DT_VCENTER);
        
        DeleteObject(hFont);
        EndPaint(hwnd, &ps);
        break;
    }
    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wParam;
        SetBkColor(hdc, RGB(30, 30, 40));
        SetTextColor(hdc, COLOR_TEXT_WHITE);
        return (LRESULT)CreateSolidBrush(RGB(30, 30, 40));
    }
    case WM_CTLCOLORBTN: {
        HDC hdc = (HDC)wParam;
        SetBkColor(hdc, COLOR_ACCENT_BLUE);
        SetTextColor(hdc, COLOR_TEXT_WHITE);
        return (LRESULT)CreateSolidBrush(COLOR_ACCENT_BLUE);
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        hSidebar = CreateWindowA("SidebarWindow", "",
            WS_VISIBLE | WS_CHILD,
            0, 0, 250, 600, hwnd, NULL, NULL, NULL);
        

        hMessageList = CreateWindowA("MessageListWindow", "",
            WS_VISIBLE | WS_CHILD | WS_VSCROLL,
            250, 0, 750, 550, hwnd, NULL, NULL, NULL);
        
        hInputEdit = CreateWindowA("EDIT", "",
            WS_VISIBLE | WS_CHILD | ES_LEFT | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL,
            250, 550, 650, 50, hwnd, NULL, NULL, NULL);
        
        hSendBtn = CreateWindowA("BUTTON", "Send",
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            900, 550, 100, 50, hwnd, (HMENU)2, NULL, NULL);
        break;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == 2) { 
            SendChatMessage();
        }
        break;
    case WM_KEYDOWN:
        if (wParam == VK_RETURN && GetFocus() == hInputEdit) {

            if (GetKeyState(VK_SHIFT) >= 0) {
                SendChatMessage();
                return 0; 
            }
        }
        break;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        
        RECT rect;
        GetClientRect(hwnd, &rect);
        HBRUSH brush = CreateSolidBrush(COLOR_BG_DARK);
        FillRect(hdc, &rect, brush);
        DeleteObject(brush);
        
        EndPaint(hwnd, &ps);
        break;
    }
    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wParam;
        SetBkColor(hdc, RGB(30, 30, 40));
        SetTextColor(hdc, COLOR_TEXT_WHITE);
        return (LRESULT)CreateSolidBrush(RGB(30, 30, 40));
    }
    case WM_CTLCOLORBTN: {
        HDC hdc = (HDC)wParam;
        SetBkColor(hdc, COLOR_ACCENT_BLUE);
        SetTextColor(hdc, COLOR_TEXT_WHITE);
        return (LRESULT)CreateSolidBrush(COLOR_ACCENT_BLUE);
    }
    case WM_SIZE: {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);
        MoveWindow(hSidebar, 0, 0, 250, height, TRUE);
        MoveWindow(hMessageList, 250, 0, width - 250, height - 50, TRUE);
        MoveWindow(hInputEdit, 250, height - 50, width - 350, 50, TRUE);
        MoveWindow(hSendBtn, width - 100, height - 50, 100, 50, TRUE);
        InvalidateRect(hwnd, NULL, TRUE);
        InvalidateRect(hSidebar, NULL, TRUE);
        InvalidateRect(hMessageList, NULL, TRUE);
        break;
    }
    case WM_DESTROY:
        isConnected = false;
        if (clientSocket != INVALID_SOCKET) {
            closesocket(clientSocket);
        }
        if (receiveThread.joinable()) {
            receiveThread.join();
        }
        WSACleanup();
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

LRESULT CALLBACK SidebarWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rect;
        GetClientRect(hwnd, &rect);
        OnPaintSidebar(hdc, rect.right, rect.bottom);
        EndPaint(hwnd, &ps);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK MessageListWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_MOUSEWHEEL: {
        int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
        scrollPos -= (zDelta / 2); // Регулируйте делитель для скорости (2-4 обычно ок)
        if (scrollPos < 0) scrollPos = 0;
        
        // Ограничение скролла снизу (чтобы не улетать в пустоту)
        RECT rect;
        GetClientRect(hwnd, &rect);
        int totalHeight = 20;
        for (const auto& m : messages) totalHeight += GetMessageHeight(m, rect.right) + 10;
        int maxScroll = std::max((int)0, (int)(totalHeight - rect.bottom + 50));
        if (scrollPos > maxScroll) scrollPos = maxScroll;

        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rect;
        GetClientRect(hwnd, &rect);

        // Двойная буферизация для отсутствия мерцания
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
        SelectObject(memDC, memBitmap);

        // Рисуем фон
        HBRUSH brush = CreateSolidBrush(COLOR_BG_DARK);
        FillRect(memDC, &rect, brush);
        DeleteObject(brush);

        // Отрисовка сообщений с учетом scrollPos
        int y = 20 - scrollPos; 
        for (const auto& msg : messages) {
            int h = GetMessageHeight(msg, rect.right);
            // Рисуем только те, что видны на экране (оптимизация)
            if (y + h > 0 && y < rect.bottom) {
                DrawMessage(memDC, msg, y, rect.right);
            }
            y += h + 10;
        }

        BitBlt(hdc, 0, 0, rect.right, rect.bottom, memDC, 0, 0, SRCCOPY);
        DeleteObject(memBitmap);
        DeleteDC(memDC);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
// Регистрация класса окна
void RegisterWindowClass(const char* className, WNDPROC proc) {
    WriteLog("Registering class: " + std::string(className));
    WNDCLASSA wc = {}; // Важно: фигурные скобки обнуляют структуру
    wc.lpfnWndProc = proc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = className;
    wc.hbrBackground = CreateSolidBrush(COLOR_BG_DARK); 
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    
    if (!RegisterClassA(&wc)) {
        WriteLog("Failed to register class: " + std::string(className) + " Error: " + std::to_string(GetLastError()));
    } else {
        WriteLog("Class " + std::string(className) + " registered successfully.");
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WriteLog("--- APP START ---"); 
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        MessageBoxA(NULL, "WSAStartup failed", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    
    RegisterWindowClass("ConnectWindow", ConnectWndProc);
    RegisterWindowClass("MainWindow", MainWndProc);
    RegisterWindowClass("SidebarWindow", SidebarWndProc);
    RegisterWindowClass("MessageListWindow", MessageListWndProc);
    
    // Окно подключения
    hConnectWnd = CreateWindowExA(0, "ConnectWindow", "AEGIS - Connection",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 320,
        NULL, NULL, GetModuleHandle(NULL), NULL);
    
    // Главное окно (скрыто до подключения)
    hMainWnd = CreateWindowExA(0, "MainWindow", "AEGIS - Chat",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1000, 600,
        NULL, NULL, GetModuleHandle(NULL), NULL);
    
    ShowWindow(hConnectWnd, nCmdShow);
    UpdateWindow(hConnectWnd);
    
    // Цикл сообщений
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    // Очистка Winsock
    WSACleanup();
    
    return 0;
}

