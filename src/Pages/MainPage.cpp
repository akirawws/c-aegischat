#include "MainPage.h"
#include "Utils/Styles.h"
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
extern HWND hInputEdit; 
extern std::vector<Message> messages;
extern std::map<std::string, std::vector<Message>> chatHistories;

const int SIDEBAR_ICONS = 72;
const int SIDEBAR_DM    = 240;

LRESULT CALLBACK MessageInputSubclass(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    if (uMsg == WM_CHAR && wParam == VK_RETURN) {
        if (!(GetKeyState(VK_SHIFT) & 0x8000)) {
            HWND hMain = (HWND)dwRefData;
            SendMessage(hMain, WM_COMMAND, MAKEWPARAM(1001, 0), 0);
            return 0;
        }
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

HWND CreateMainPage(HINSTANCE hInstance, int x, int y, int width, int height) {
    hMainWnd = CreateWindowExA(
        0, "MainWindow", "AEGIS - Chat",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        x, y, width, height,
        NULL, NULL, hInstance, NULL
    );
    return hMainWnd;
}

void ShowChatUI(bool show) {
    if (hInputEdit) {
        ShowWindow(hInputEdit, show ? SW_SHOW : SW_HIDE);
    }
}

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {

    case WM_COMMAND: {
        if (LOWORD(wParam) == 1001) {
            SendPrivateMessageFromUI();
        }
        break;
    }

    case WM_CREATE: {
        int startX = SIDEBAR_ICONS + SIDEBAR_DM;
        CreateMessageInput(hwnd, startX + 10, 100, 100, INPUT_MIN_HEIGHT);
        
        if (hInputEdit) {
            SetWindowSubclass(hInputEdit, MessageInputSubclass, 0, (DWORD_PTR)hwnd);
        }

        g_uiState.currentPage = AppPage::Friends;
        g_uiState.activeChatUser = "";
        
        ShowChatUI(false); 
        return 0;
    }

    case WM_LBUTTONDOWN: {
        int x = LOWORD(lParam);
        int y = HIWORD(lParam);
        AppPage oldPage = g_uiState.currentPage;

        if (x >= SIDEBAR_ICONS && x <= SIDEBAR_ICONS + SIDEBAR_DM) {
            HandleSidebarFriendsClick(hwnd, x, y);
            
            if (oldPage != g_uiState.currentPage) {
                // Показываем инпут ТОЛЬКО если мы в личных сообщениях
                bool isMessagePage = (g_uiState.currentPage == AppPage::Messages);
                ShowChatUI(isMessagePage); 
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

        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
        HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

        // 1. Общий фон
        HBRUSH hBg = CreateSolidBrush(RGB(32, 34, 37)); 
        FillRect(memDC, &rect, hBg);
        DeleteObject(hBg);

        // 2. Сайдбары
        OnPaintSidebar(memDC, SIDEBAR_ICONS, rect.bottom);
        DrawSidebarFriends(memDC, hwnd, SIDEBAR_ICONS, 0, SIDEBAR_DM, rect.bottom);

        // 3. Контент
        int contentX = SIDEBAR_ICONS + SIDEBAR_DM;
        int contentW = rect.right - contentX;
        int contentH = rect.bottom;

        if (g_uiState.currentPage == AppPage::Friends) {
            DrawFriendsPage(memDC, hwnd, rect.right, rect.bottom); 
        } 
        else if (g_uiState.currentPage == AppPage::FriendRequests) {
            // ЗДЕСЬ ПУСТО: Сообщения не отображаются
            // Если у вас появится функция DrawRequestsPage, вызывайте её здесь
        }
        else if (g_uiState.currentPage == AppPage::Messages) {
            // Личные сообщения
            if (chatHistories.count(g_uiState.activeChatUser)) {
                DrawMessagePage(memDC, contentX, 0, contentW, contentH, chatHistories[g_uiState.activeChatUser]);
            } else {
                std::vector<Message> empty;
                DrawMessagePage(memDC, contentX, 0, contentW, contentH, empty);
            }
        }

        BitBlt(hdc, 0, 0, rect.right, rect.bottom, memDC, 0, 0, SRCCOPY);

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

        if (hInputEdit) {
            MoveWindow(hInputEdit, chatX + 20, height - inputEditHeight - 20, chatW - 40, inputEditHeight, TRUE);
        }
        InvalidateRect(hwnd, NULL, FALSE);
        break;
    }
    
    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}