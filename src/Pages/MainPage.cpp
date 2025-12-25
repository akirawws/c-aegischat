#include "MainPage.h"
#include "Utils/Styles.h"
#include "Components/MessageList.h"
#include "Components/MessageInput.h"
#include "Components/Sidebar.h"
#include "Pages/FriendsPage.h"
#include "Pages/MessagePage.h"
#include "Components/SidebarFriends.h"
#include "Utils/Keyboard.h"
#include "Utils/Network.h"
#include "Utils/UIState.h"
#include <map>
#include <vector>
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")

HWND hMainWnd = NULL;
extern int inputEditHeight;
extern HWND hMessageList;
extern HWND hInputEdit;
extern std::vector<Message> messages;
extern std::map<std::string, std::vector<Message>> chatHistories;
const int SIDEBAR_ICONS = 72;
const int SIDEBAR_DM    = 240;

// Утилита для проверки: рисуется ли область вообще
void DrawDebugRect(HDC hdc, int x, int y, int w, int h, COLORREF color) {
    RECT r = { x, y, x + w, y + h };
    HBRUSH br = CreateSolidBrush(color);
    FillRect(hdc, &r, br);
    DeleteObject(br);
}

LRESULT CALLBACK MessageInputSubclass(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    if (uMsg == WM_CHAR && wParam == VK_RETURN) {
        // Если нажат Enter (и не нажат Shift)
        if (!(GetKeyState(VK_SHIFT) & 0x8000)) {
            HWND hMain = (HWND)dwRefData;
            // Посылаем сигнал родителю выполнить отправку
            SendMessage(hMain, WM_COMMAND, MAKEWPARAM(1001, 0), 0);
            return 0; // Блокируем стандартный звук "писк" Windows на Enter
        }
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

HWND CreateMainPage(HINSTANCE hInstance, int x, int y, int width, int height) {
    hMainWnd = CreateWindowExA(
        0, "MainWindow", "AEGIS - Chat",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, // ВАЖНО: WS_CLIPCHILDREN убирает мерцание под дочерними окнами
        x, y, width, height,
        NULL, NULL, hInstance, NULL
    );
    return hMainWnd;
}

void ShowChatUI(bool show) {
    int cmd = show ? SW_SHOW : SW_HIDE;
    if (hMessageList) ShowWindow(hMessageList, cmd);
    if (hInputEdit)   ShowWindow(hInputEdit, cmd);
}

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        

    case WM_COMMAND: {
        if (LOWORD(wParam) == 1001) {
            OutputDebugStringA("ENTER PRESSED -> SEND\n");
            SendPrivateMessageFromUI();
        }
        break;
    }


case WM_CREATE: {
    int startX = SIDEBAR_ICONS + SIDEBAR_DM;
    // Создаем чат СРАЗУ с правильными координатами, чтобы он не перекрывал лево
    CreateMessageList(hwnd, startX, 0, 100, 100); 
    CreateMessageInput(hwnd, startX + 10, 100, 100, INPUT_MIN_HEIGHT);
    SetFocus(hInputEdit);

    
    
    // Инициализация состояния UI
    g_uiState.currentPage = AppPage::Friends;
    g_uiState.activeChatUser = "";
    

    // Отладочный вывод
    OutputDebugStringA("Main window created with test users\n");
    
    ShowChatUI(false); 
    if (hInputEdit) {
        SetWindowSubclass(hInputEdit, MessageInputSubclass, 0, (DWORD_PTR)hwnd);
    }
    return 0;
}

case WM_LBUTTONDOWN: {
    int x = LOWORD(lParam);
    int y = HIWORD(lParam);
    AppPage oldPage = g_uiState.currentPage;

    if (x < SIDEBAR_ICONS) {
    }

    else if (x >= SIDEBAR_ICONS && x <= SIDEBAR_ICONS + SIDEBAR_DM) {
        HandleSidebarFriendsClick(hwnd, x, y);
        
        if (oldPage != g_uiState.currentPage) {
            ShowChatUI(g_uiState.currentPage != AppPage::Friends);
            InvalidateRect(hwnd, NULL, FALSE);
        }
    }
    else {
        if (g_uiState.currentPage == AppPage::Friends) {
            HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
            HandleFriendsClick(hwnd, x, y, hInstance);
        }
    }
    return 0;
}

case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rect;
        GetClientRect(hwnd, &rect);

        // 1. Подготовка Double Buffering
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
        HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

        // 2. Фон
        HBRUSH hBg = CreateSolidBrush(COLOR_BG_DARK);
        FillRect(memDC, &rect, hBg);
        DeleteObject(hBg);

        // 3. Отрисовка Сайдбаров
        OnPaintSidebar(memDC, SIDEBAR_ICONS, rect.bottom);
        DrawSidebarFriends(memDC, hwnd, SIDEBAR_ICONS, 0, SIDEBAR_DM, rect.bottom);

        // 4. Отрисовка Контента (Правая часть)
        int contentX = SIDEBAR_ICONS + SIDEBAR_DM; // 312 пикселей отступ
        int contentW = rect.right - contentX;
        int contentH = rect.bottom;

        if (g_uiState.currentPage == AppPage::Friends) {
            DrawFriendsPage(memDC, hwnd, rect.right, rect.bottom); 
        } 
        else {
            // ИСПРАВЛЕНИЕ: Используем вычисленные координаты и правильный вектор сообщений
            // Если у вас выбран конкретный пользователь, передаем его историю:
            if (chatHistories.count(g_uiState.activeChatUser)) {
                DrawMessagePage(memDC, contentX, 0, contentW, contentH, chatHistories[g_uiState.activeChatUser]);
            } else {
                // Если истории еще нет или это общий чат
                DrawMessagePage(memDC, contentX, 0, contentW, contentH, messages);
            }
        }

        // 5. Копирование из буфера на экран
        BitBlt(hdc, 0, 0, rect.right, rect.bottom, memDC, 0, 0, SRCCOPY);

        // Чистка ресурсов
        SelectObject(memDC, oldBitmap);
        DeleteObject(memBitmap);
        DeleteDC(memDC);
        EndPaint(hwnd, &ps);
        break;
    }

    case WM_SIZE: {
        int width  = LOWORD(lParam);
        int height = HIWORD(lParam);
        int chatX  = SIDEBAR_ICONS + SIDEBAR_DM;
        int chatW  = width - chatX;

        if (hMessageList) {
            MoveWindow(hMessageList, chatX, 0, chatW, height - inputEditHeight - 20, TRUE);
        }
        if (hInputEdit) {
            MoveWindow(hInputEdit, chatX + 15, height - inputEditHeight - 10, chatW - 30, inputEditHeight, TRUE);
        }
        InvalidateRect(hwnd, NULL, FALSE);
        break;
    }
    
    case WM_DESTROY:
        isConnected = false;
        if (clientSocket != INVALID_SOCKET) closesocket(clientSocket);
        if (receiveThread.joinable()) receiveThread.join();
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

