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
#include <richedit.h>
#pragma comment(lib, "Msimg32.lib")
#ifndef NOMINMAX
#define NOMINMAX
#endif

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "comctl32.lib")

// Цвета темы (современная темная палитра)
#define COLOR_BG_DARK RGB(16, 16, 20)
#define COLOR_BG_SIDEBAR RGB(22, 22, 28)
#define COLOR_BG_BLUE RGB(35, 55, 85)
#define COLOR_BG_BLUE_HOVER RGB(45, 70, 110)
#define COLOR_TEXT_WHITE RGB(245, 245, 250)
#define COLOR_TEXT_GRAY RGB(170, 170, 180)
#define COLOR_TEXT_LIGHT_GRAY RGB(200, 200, 210)
#define COLOR_ACCENT_BLUE RGB(80, 140, 220)
#define COLOR_ACCENT_BLUE_DARK RGB(60, 110, 180)
#define COLOR_ACCENT_BLUE_LIGHT RGB(100, 160, 240)
#define COLOR_MESSAGE_OTHER RGB(30, 38, 50)
#define COLOR_MESSAGE_USER RGB(55, 95, 150)
#define COLOR_INPUT_BG RGB(28, 28, 34)
#define COLOR_INPUT_BORDER RGB(45, 45, 55)
#define COLOR_BUTTON_HOVER RGB(90, 150, 230)
#define FONT_FAMILY "Segoe UI"
#define FONT_SIZE_LARGE 24
#define FONT_SIZE_MEDIUM 16
#define FONT_SIZE_NORMAL 14
#define FONT_SIZE_SMALL 12
#define FONT_WEIGHT_NORMAL FW_NORMAL
#define FONT_WEIGHT_SEMIBOLD FW_SEMIBOLD
#define FONT_WEIGHT_BOLD FW_BOLD
int scrollPos = 0;



struct Message {
    std::string text;
    bool isUser;
    std::string sender;
    std::string timeStr;
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
int inputEditHeight = 60; 
const int INPUT_MIN_HEIGHT = 60;
const int INPUT_MAX_HEIGHT = 200;
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
// Функция создания единого шрифта
HFONT CreateAppFont(int size, int weight = FONT_WEIGHT_NORMAL) {
    return CreateFontA(size, 0, 0, 0, weight, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, FONT_FAMILY);
}

// Функция создания шрифта с увеличенным межстрочным интервалом для поля ввода
HFONT CreateInputFont(int size, int weight = FONT_WEIGHT_NORMAL) {
    HDC hdc = GetDC(NULL);
    int logPixels = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(NULL, hdc);
    
    // Увеличиваем высоту шрифта для большего межстрочного интервала
    int height = -MulDiv(size + 2, logPixels, 72); // +2 для дополнительного пространства
    
    LOGFONTA lf = {};
    lf.lfHeight = height;
    lf.lfWeight = weight;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = CLEARTYPE_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    strcpy_s(lf.lfFaceName, FONT_FAMILY);
    
    return CreateFontIndirectA(&lf);
}

// Вычисление высоты сообщения (объявление перед использованием)
int GetMessageHeight(const Message& msg, int width) {
    HDC hdc = GetDC(hMessageList);
    int msgWidth = (int)(width * 0.70);
    RECT textRect = { 0, 0, msgWidth - 32, 0 };
    
    HFONT hFont = CreateAppFont(FONT_SIZE_NORMAL);
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    
    // Расчет размеров текста
    DrawTextA(hdc, msg.text.c_str(), -1, &textRect, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_CALCRECT);
    
    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
    ReleaseDC(hMessageList, hdc);

    return std::max((int)70, (int)(textRect.bottom + 50));
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

    int tLen = MultiByteToWideChar(CP_UTF8, 0, msg.timeStr.c_str(), -1, NULL, 0);
    std::wstring wTime(tLen, 0);
    MultiByteToWideChar(CP_UTF8, 0, msg.timeStr.c_str(), -1, &wTime[0], tLen);

    // 2. Расчет размеров бабла
    int maxBubbleWidth = (int)(width * 0.70);
    
    HFONT hFontMsg = CreateFontW(FONT_SIZE_NORMAL, 0, 0, 0, FONT_WEIGHT_NORMAL, FALSE, FALSE, FALSE, 
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, 
                                 CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    
    HDC hdcMeasure = CreateCompatibleDC(hdc);
    SelectObject(hdcMeasure, hFontMsg);
    
    RECT calcRect = { 0, 0, maxBubbleWidth - 36, 0 };
    DrawTextW(hdcMeasure, wText.c_str(), -1, &calcRect, DT_CALCRECT | DT_WORDBREAK);
    DeleteDC(hdcMeasure);

    int finalBubbleWidth = std::max((int)160, (int)calcRect.right + 36);
    int finalHeight = calcRect.bottom + 52; 
    
    int x = msg.isUser ? (width - finalBubbleWidth - 20) : 20;

    // 3. Отрисовка "Бабла" с тенью
    // Тень
    HBRUSH shadowBrush = CreateSolidBrush(RGB(0, 0, 0));
    RECT shadowRect = { x + 2, y + 2, x + finalBubbleWidth + 2, y + finalHeight + 2 };
    FillRect(hdc, &shadowRect, shadowBrush);
    DeleteObject(shadowBrush);
    
    // Основной бабл
    HPEN borderPen = CreatePen(PS_SOLID, 1, msg.isUser ? COLOR_ACCENT_BLUE : COLOR_INPUT_BORDER);
    HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
    HBRUSH bubbleBrush = CreateSolidBrush(msg.isUser ? COLOR_MESSAGE_USER : COLOR_MESSAGE_OTHER);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, bubbleBrush);
    
    RoundRect(hdc, x, y, x + finalBubbleWidth, y + finalHeight, 18, 18);
    
    SelectObject(hdc, oldPen);
    DeleteObject(borderPen);

    // 4. Отрисовка ника и времени
    SetBkMode(hdc, TRANSPARENT);
    
    // Шрифт ника
    HFONT hFontName = CreateFontW(FONT_SIZE_SMALL, 0, 0, 0, FONT_WEIGHT_SEMIBOLD, FALSE, FALSE, FALSE, 
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, 
                                  CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    SelectObject(hdc, hFontName);
    SetTextColor(hdc, msg.isUser ? COLOR_ACCENT_BLUE_LIGHT : COLOR_ACCENT_BLUE);
    
    // Рисуем ник
    TextOutW(hdc, x + 18, y + 12, wSender.c_str(), (int)wcslen(wSender.c_str()));

    SIZE nameSize;
    GetTextExtentPoint32W(hdc, wSender.c_str(), (int)wcslen(wSender.c_str()), &nameSize);

    // Шрифт времени
    HFONT hFontTime = CreateFontW(FONT_SIZE_SMALL - 1, 0, 0, 0, FONT_WEIGHT_NORMAL, FALSE, FALSE, FALSE, 
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, 
                                  CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    SelectObject(hdc, hFontTime);
    SetTextColor(hdc, COLOR_TEXT_GRAY);
    
    TextOutW(hdc, x + 18 + nameSize.cx + 10, y + 13, wTime.c_str(), (int)wcslen(wTime.c_str()));

    // 5. Сообщение
    SelectObject(hdc, hFontMsg);
    SetTextColor(hdc, COLOR_TEXT_WHITE);
    RECT textRect = { x + 18, y + 32, x + finalBubbleWidth - 18, y + finalHeight - 12 };
    DrawTextW(hdc, wText.c_str(), -1, &textRect, DT_LEFT | DT_WORDBREAK | DT_TOP);

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
    
    // Фон
    HBRUSH brush = CreateSolidBrush(COLOR_BG_SIDEBAR);
    FillRect(hdc, &rect, brush);
    DeleteObject(brush);
    
    // Заголовок
    SetTextColor(hdc, COLOR_TEXT_WHITE);
    SetBkMode(hdc, TRANSPARENT);
    HFONT hFont = CreateAppFont(FONT_SIZE_MEDIUM, FONT_WEIGHT_BOLD);
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    
    RECT textRect = { 24, 24, width - 24, 56 };
    DrawTextA(hdc, "Chats", -1, &textRect, DT_LEFT | DT_VCENTER);
    
    // Разделительная линия
    HPEN linePen = CreatePen(PS_SOLID, 1, COLOR_INPUT_BORDER);
    HPEN oldPen = (HPEN)SelectObject(hdc, linePen);
    MoveToEx(hdc, 16, 68, NULL);
    LineTo(hdc, width - 16, 68);
    SelectObject(hdc, oldPen);
    DeleteObject(linePen);
    
    // Активный чат
    RECT chatRect = { 12, 80, width - 12, 120 };
    brush = CreateSolidBrush(COLOR_BG_BLUE);
    FillRect(hdc, &chatRect, brush);
    DeleteObject(brush);
    
    // Рамка активного чата
    HPEN borderPen = CreatePen(PS_SOLID, 1, COLOR_ACCENT_BLUE);
    SelectObject(hdc, borderPen);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    RoundRect(hdc, chatRect.left, chatRect.top, chatRect.right, chatRect.bottom, 8, 8);
    SelectObject(hdc, oldPen);
    DeleteObject(borderPen);
    
    // Текст чата
    SetTextColor(hdc, COLOR_TEXT_WHITE);
    SelectObject(hdc, hOldFont);
    hFont = CreateAppFont(FONT_SIZE_NORMAL, FONT_WEIGHT_SEMIBOLD);
    SelectObject(hdc, hFont);
    
    textRect = { 24, 88, width - 24, 112 };
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
        int fieldW = 300;
        int fieldH = 35; // Немного увеличим высоту для крупного текста
        int startX = (420 - fieldW) / 2 - 10;

        hIPEdit = CreateWindowA("EDIT", "xisyrurdm.localto.net",
            WS_VISIBLE | WS_CHILD | ES_LEFT | WS_BORDER | ES_AUTOHSCROLL,
            startX, 120, fieldW, fieldH, hwnd, NULL, NULL, NULL);

        hPortEdit = CreateWindowA("EDIT", "6162",
            WS_VISIBLE | WS_CHILD | ES_LEFT | WS_BORDER | ES_AUTOHSCROLL,
            startX, 185, fieldW, fieldH, hwnd, NULL, NULL, NULL);

        hNameEdit = CreateWindowA("EDIT", "",
            WS_VISIBLE | WS_CHILD | ES_LEFT | WS_BORDER | ES_AUTOHSCROLL,
            startX, 250, fieldW, fieldH, hwnd, NULL, NULL, NULL);

        // Добавляем BS_OWNERDRAW, чтобы сделать кнопку синей
        hConnectBtn = CreateWindowA("BUTTON", "Connect",
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 
            startX, 320, fieldW, 45, hwnd, (HMENU)1, NULL, NULL);
        
        // Создаем крупный шрифт для полей ввода (размер 20)
        HFONT hEditFont = CreateAppFont(20, FONT_WEIGHT_NORMAL);
        SendMessage(hIPEdit, WM_SETFONT, (WPARAM)hEditFont, TRUE);
        SendMessage(hPortEdit, WM_SETFONT, (WPARAM)hEditFont, TRUE);
        SendMessage(hNameEdit, WM_SETFONT, (WPARAM)hEditFont, TRUE);
        break;
    }

    case WM_DRAWITEM: {
        if (wParam == 1) { // Отрисовка кнопки Connect
            LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
            HDC hdc = dis->hDC;
            RECT rect = dis->rcItem;

            // Определяем цвет (темнее при нажатии)
            HBRUSH hBtnBrush = CreateSolidBrush((dis->itemState & ODS_SELECTED) ? 
                               COLOR_ACCENT_BLUE_DARK : COLOR_ACCENT_BLUE);
            
            // Рисуем скругленный прямоугольник или обычный
            FillRect(hdc, &rect, hBtnBrush);
            DeleteObject(hBtnBrush);

            // Текст кнопки
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, COLOR_TEXT_WHITE);
            HFONT hBtnFont = CreateAppFont(18, FONT_WEIGHT_BOLD);
            HFONT hOldFont = (HFONT)SelectObject(hdc, hBtnFont);
            
            DrawTextA(hdc, "Connect", -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            
            SelectObject(hdc, hOldFont);
            DeleteObject(hBtnFont);
            return TRUE;
        }
        break;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == 1) { 
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
            WriteLog("Connect button clicked. Target: " + std::string(ip) + ":" + std::string(port));

            if (ConnectToServer(ip, port)) {
                ShowWindow(hwnd, SW_HIDE);
                ShowWindow(hMainWnd, SW_SHOW);
            } else {
                int lastError = WSAGetLastError();
                MessageBoxW(hwnd, L"Ошибка подключения! Проверьте данные.", L"Error", MB_OK | MB_ICONERROR);
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
        HFONT hFontLog = CreateAppFont(FONT_SIZE_LARGE, FONT_WEIGHT_BOLD);
        SelectObject(hdc, hFontLog);
        RECT textRect = { 0, 32, rect.right, 72 };
        DrawTextA(hdc, "AEGIS", -1, &textRect, DT_CENTER | DT_VCENTER);
        DeleteObject(hFontLog);

        HFONT hLabelFont = CreateAppFont(14, FONT_WEIGHT_NORMAL);
        SelectObject(hdc, hLabelFont);
        SetTextColor(hdc, COLOR_TEXT_GRAY);
        
        // Метки
        textRect = { 55, 100, 350, 120 }; DrawTextA(hdc, "Server IP address", -1, &textRect, DT_LEFT);
        textRect = { 55, 165, 350, 185 }; DrawTextA(hdc, "Port", -1, &textRect, DT_LEFT);
        textRect = { 55, 230, 350, 250 }; DrawTextA(hdc, "Your name", -1, &textRect, DT_LEFT);
        
        DeleteObject(hLabelFont);
        EndPaint(hwnd, &ps);
        break;
    }

    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wParam;
        SetBkColor(hdc, COLOR_INPUT_BG);
        SetTextColor(hdc, COLOR_TEXT_WHITE);
        // Чтобы текст не "прилипал" к краям, можно добавить отступы, 
        // но для простоты используем сплошной цвет фона
        static HBRUSH hEditBg = CreateSolidBrush(COLOR_INPUT_BG);
        return (LRESULT)hEditBg;
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
        
        // Создаем поле ввода без стандартной рамки (будем рисовать сами)
        // Убираем ES_WANTRETURN, чтобы контролировать Enter вручную
        hInputEdit = CreateWindowA("EDIT", "",
            WS_VISIBLE | WS_CHILD | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL,
            260, 550, 740, INPUT_MIN_HEIGHT, hwnd, NULL, NULL, NULL);
        
        // Устанавливаем увеличенный шрифт с увеличенным межстрочным интервалом
        HFONT hInputFont = CreateInputFont(14, FONT_WEIGHT_NORMAL);
        SendMessage(hInputEdit, WM_SETFONT, (WPARAM)hInputFont, TRUE);
        
        // Устанавливаем отступы для текста
        SendMessage(hInputEdit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(16, 16));
        
        break;
    }
case WM_COMMAND:
        if (HIWORD(wParam) == EN_CHANGE && (HWND)lParam == hInputEdit) {
            // Принудительно перерисовываем родительское окно, чтобы обновить рамку и фон
            InvalidateRect(hwnd, NULL, FALSE); 
            
            // Автоматическое изменение высоты поля ввода
            RECT rect;
            GetClientRect(hInputEdit, &rect);
            HDC hdc = GetDC(hInputEdit);
            HFONT hFont = CreateInputFont(18, FONT_WEIGHT_NORMAL);
            HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
            
            int len = GetWindowTextLengthW(hInputEdit);
            std::vector<wchar_t> buffer(len + 1);
            GetWindowTextW(hInputEdit, buffer.data(), len + 1);
            
            RECT calcRect = { 0, 0, rect.right - 32, 0 };
            DrawTextW(hdc, buffer.data(), -1, &calcRect, DT_CALCRECT | DT_WORDBREAK | DT_EDITCONTROL);
            
            int textHeight = calcRect.bottom;
            int newHeight = std::max(INPUT_MIN_HEIGHT, std::min(INPUT_MAX_HEIGHT, textHeight + 32));
            
            SelectObject(hdc, hOldFont);
            DeleteObject(hFont);
            ReleaseDC(hInputEdit, hdc);
            
            if (newHeight != inputEditHeight) {
                inputEditHeight = newHeight;
                RECT mainRect;
                GetClientRect(hwnd, &mainRect);
                MoveWindow(hInputEdit, 260, mainRect.bottom - inputEditHeight - 10, 
                          mainRect.right - 270, inputEditHeight, TRUE);
                MoveWindow(hMessageList, 250, 0, mainRect.right - 250, 
                          mainRect.bottom - inputEditHeight - 20, TRUE);
                InvalidateRect(hwnd, NULL, TRUE);
            }
        }
        break;

    case WM_CTLCOLOREDIT: {
            if ((HWND)lParam == hInputEdit) {
                HDC hdc = (HDC)wParam;
                // Устанавливаем непрозрачный режим и цвет фона
                SetBkMode(hdc, OPAQUE); 
                SetBkColor(hdc, COLOR_INPUT_BG);
                SetTextColor(hdc, COLOR_TEXT_WHITE);
                
                static HBRUSH hInputBgBrush = CreateSolidBrush(COLOR_INPUT_BG);
                return (LRESULT)hInputBgBrush;
            }
            break;
        }
    case WM_KEYDOWN:
        // Обработка стандартных горячих клавиш
        if (GetFocus() == hInputEdit) {
            bool ctrl = GetKeyState(VK_CONTROL) < 0;
            bool shift = GetKeyState(VK_SHIFT) < 0;
            
            if (ctrl) {
                switch (wParam) {
                case 'A': case 'a': // Ctrl+A - выделить все
                    SendMessage(hInputEdit, EM_SETSEL, 0, -1);
                    return 0;
                case 'C': case 'c': // Ctrl+C - копировать
                    SendMessage(hInputEdit, WM_COPY, 0, 0);
                    return 0;
                case 'V': case 'v': // Ctrl+V - вставить
                    SendMessage(hInputEdit, WM_PASTE, 0, 0);
                    return 0;
                case 'X': case 'x': // Ctrl+X - вырезать
                    SendMessage(hInputEdit, WM_CUT, 0, 0);
                    return 0;
                case 'Z': case 'z': // Ctrl+Z - отменить
                    if (shift) {
                        SendMessage(hInputEdit, EM_REDO, 0, 0); // Ctrl+Shift+Z - повторить
                    } else {
                        SendMessage(hInputEdit, EM_UNDO, 0, 0);
                    }
                    return 0;
                }
            }
            
            // Enter без Shift - отправка (блокируем стандартную обработку)
            if (wParam == VK_RETURN && !shift) {
                SendChatMessage();
                return 0; // Блокируем стандартную обработку Enter
            }
            // Shift+Enter - разрешаем стандартную обработку (новая строка)
        }
        break;
    case WM_CHAR:
        // Дополнительная обработка Enter для надежности
        if (wParam == VK_RETURN && GetFocus() == hInputEdit) {
            bool shift = GetKeyState(VK_SHIFT) < 0;
            if (!shift) {
                // Enter без Shift - отправка, блокируем вставку символа новой строки
                SendChatMessage();
                return 0;
            }
            // Shift+Enter - разрешаем стандартную обработку (новая строка)
        }
        break;
    case WM_SETFOCUS:
        if ((HWND)wParam == hInputEdit || GetFocus() == hInputEdit) {
            InvalidateRect(hwnd, NULL, TRUE);
        }
        break;
    case WM_KILLFOCUS:
        if ((HWND)wParam == hInputEdit || GetFocus() != hInputEdit) {
            InvalidateRect(hwnd, NULL, TRUE);
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
        
        // Рисуем поле ввода с круглыми углами
        if (hInputEdit) {
            RECT inputRect;
            GetWindowRect(hInputEdit, &inputRect);
            ScreenToClient(hwnd, (LPPOINT)&inputRect);
            ScreenToClient(hwnd, ((LPPOINT)&inputRect) + 1);
            
            // Расширяем область для отрисовки рамки
            inputRect.left -= 2;
            inputRect.top -= 2;
            inputRect.right += 2;
            inputRect.bottom += 2;
            
            // Проверяем, есть ли фокус
            bool hasFocus = (GetFocus() == hInputEdit);
            
            // Фон поля ввода
            HBRUSH inputBrush = CreateSolidBrush(COLOR_INPUT_BG);
            HPEN inputPen = CreatePen(PS_SOLID, hasFocus ? 2 : 1, 
                                     hasFocus ? COLOR_ACCENT_BLUE : COLOR_INPUT_BORDER);
            HPEN oldPen = (HPEN)SelectObject(hdc, inputPen);
            HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, inputBrush);
            
            // Рисуем скругленный прямоугольник
            RoundRect(hdc, inputRect.left, inputRect.top, inputRect.right, inputRect.bottom, 14, 14);
            
            SelectObject(hdc, oldPen);
            SelectObject(hdc, oldBrush);
            DeleteObject(inputBrush);
            DeleteObject(inputPen);
        }
        
        EndPaint(hwnd, &ps);
        break;
    }
    case WM_SIZE: {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);
        MoveWindow(hSidebar, 0, 0, 250, height, TRUE);
        MoveWindow(hMessageList, 250, 0, width - 250, height - inputEditHeight - 20, TRUE);
        MoveWindow(hInputEdit, 260, height - inputEditHeight - 10, width - 270, inputEditHeight, TRUE);
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
    LoadLibraryA("Msftedit.dll");
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return 1;

    // Сначала РЕГИСТРИРУЕМ все классы
    RegisterWindowClass("ConnectWindow", ConnectWndProc);
    RegisterWindowClass("MainWindow", MainWndProc);
    RegisterWindowClass("SidebarWindow", SidebarWndProc);
    RegisterWindowClass("MessageListWindow", MessageListWndProc);
    
    // Параметры окна подключения
    int connWidth = 420;
    int connHeight = 450; 
    int screenX = (GetSystemMetrics(SM_CXSCREEN) - connWidth) / 2;
    int screenY = (GetSystemMetrics(SM_CYSCREEN) - connHeight) / 2;

    // Создаем окно подключения ПОСЛЕ регистрации класса
    hConnectWnd = CreateWindowExA(0, "ConnectWindow", "AEGIS - Connection",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        screenX, screenY, connWidth, connHeight,
        NULL, NULL, hInstance, NULL);

    // Главное окно
    hMainWnd = CreateWindowExA(0, "MainWindow", "AEGIS - Chat",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1000, 600,
        NULL, NULL, hInstance, NULL);
    
    if (!hConnectWnd) {
        WriteLog("Critical Error: hConnectWnd is NULL. LastError: " + std::to_string(GetLastError()));
        return 0;
    }

    ShowWindow(hConnectWnd, nCmdShow);
    UpdateWindow(hConnectWnd);
    
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        // Перехват Enter для поля ввода сообщения
        if (msg.message == WM_KEYDOWN && msg.hwnd == hInputEdit) {
            if (msg.wParam == VK_RETURN) {
                // Если Shift НЕ зажат — отправляем сообщение
                if (!(GetKeyState(VK_SHIFT) & 0x8000)) {
                    SendChatMessage();
                    continue; // Пропускаем TranslateMessage, чтобы не добавлять перевод строки
                }
            }
        }
        
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    WSACleanup();
    return 0;
}
