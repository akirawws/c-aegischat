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
#include <gdiplus.h>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "Msimg32.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "comctl32.lib")

#include "Utils/Styles.h"
#include "Utils/Utils.h"
#include "Utils/Network.h"
#include "Utils/Keyboard.h"
#include "Components/Sidebar.h"
#include "Components/MessageList.h"
#include "Components/MessageInput.h"
#include "Pages/AuthPage.h"
#include "Pages/MainPage.h"

void RegisterWindowClass(const char* className, WNDPROC proc) {
    WriteLog("Registering class: " + std::string(className));
    WNDCLASSA wc = {};
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

    // 1. Инициализация Winsock (ОДИН РАЗ)
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        WriteLog("Failed to startup Winsock");
        return 1;
    }

    // 2. Инициализация GDI+
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    // 3. Загрузка библиотек и регистрация классов
    LoadLibraryA("Msftedit.dll");
    RegisterWindowClass("AuthWindow", AuthWndProc);
    RegisterWindowClass("MainWindow", MainWndProc);
    RegisterWindowClass("SidebarWindow", SidebarWndProc);
    RegisterWindowClass("MessageListWindow", MessageListWndProc);
    
    // 4. Расчет координат экрана
    int connWidth = 420;
    int connHeight = 450; 
    int screenX = (GetSystemMetrics(SM_CXSCREEN) - connWidth) / 2;
    int screenY = (GetSystemMetrics(SM_CYSCREEN) - connHeight) / 2;

    // 5. Создание страницы авторизации
    CreateAuthPage(hInstance, screenX, screenY, connWidth, connHeight);
    
    if (!hAuthWnd) {
        WriteLog("Critical Error: hAuthWnd is NULL. LastError: " + std::to_string(GetLastError()));
        Gdiplus::GdiplusShutdown(gdiplusToken);
        WSACleanup();
        return 0;
    }

    ShowWindow(hAuthWnd, nCmdShow);
    UpdateWindow(hAuthWnd);
    
    // 6. ОСНОВНОЙ ЦИКЛ СООБЩЕНИЙ (Программа работает здесь)
    ProcessMessageLoop();

    // 7. ОЧИСТКА РЕСУРСОВ (Вызывается только при закрытии приложения)
    Gdiplus::GdiplusShutdown(gdiplusToken);
    WSACleanup();
    
    WriteLog("--- APP END ---");
    return 0;
}