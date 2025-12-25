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
    WSADATA wsaData;
    LoadLibraryA("Msftedit.dll");
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return 1;

    RegisterWindowClass("AuthWindow", AuthWndProc);
    RegisterWindowClass("MainWindow", MainWndProc);
    RegisterWindowClass("SidebarWindow", SidebarWndProc);
    RegisterWindowClass("MessageListWindow", MessageListWndProc);
    
    int connWidth = 420;
    int connHeight = 450; 
    int screenX = (GetSystemMetrics(SM_CXSCREEN) - connWidth) / 2;
    int screenY = (GetSystemMetrics(SM_CYSCREEN) - connHeight) / 2;

    CreateAuthPage(hInstance, screenX, screenY, connWidth, connHeight);
    // УБРАТЬ: CreateMainPage(hInstance, CW_USEDEFAULT, CW_USEDEFAULT, 1000, 600);
    // Главное окно будет создано позже, после успешной авторизации
    
    if (!hAuthWnd) {
        WriteLog("Critical Error: hAuthWnd is NULL. LastError: " + std::to_string(GetLastError()));
        return 0;
    }

    ShowWindow(hAuthWnd, nCmdShow);
    UpdateWindow(hAuthWnd);
    
    ProcessMessageLoop();
    WSACleanup();
    return 0;
}