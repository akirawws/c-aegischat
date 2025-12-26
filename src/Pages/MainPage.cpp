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
#include <gdiplus.h>
using namespace Gdiplus; 


#pragma comment(lib, "comctl32.lib")
std::vector<Message> messages; 
std::string activeChatUser = ""; 

// Глобальные переменные (внешние)
HWND hMainWnd = NULL;
extern int inputEditHeight;
extern HWND hInputEdit; 
extern HWND hMessageList;
extern std::map<std::string, std::vector<Message>> chatHistories;
extern std::vector<Message> messages; // Тот вектор, который рисует MessageList

const int SIDEBAR_ICONS = 72;
const int SIDEBAR_DM    = 240;

// Сабклассинг для Enter в поле ввода
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
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, // WS_CLIPCHILDREN важен!
        x, y, width, height,
        NULL, NULL, hInstance, NULL
    );
    return hMainWnd;
}

void ShowChatUI(bool show) {
    int cmd = show ? SW_SHOW : SW_HIDE;
    if (hInputEdit) ShowWindow(hInputEdit, cmd);
    if (hMessageList) ShowWindow(hMessageList, cmd); 
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
            
            CreateSidebar(hwnd, 0, 0, SIDEBAR_ICONS, 1000); 

            // Создаем список сообщений
            hMessageList = CreateWindowExA(
                0, "MessageListWindow", NULL,
                WS_CHILD | WS_VISIBLE | WS_VSCROLL, 
                startX, 0, 100, 100, 
                hwnd, NULL, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL
            );

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
            // 1. Обрабатываем клик по другу
            HandleSidebarFriendsClick(hwnd, x, y);
            
            // 2. Если мы переключились на сообщения — синхронизируем данные
            if (g_uiState.currentPage == AppPage::Messages) {
                // Копируем историю конкретного друга в активный вектор для отрисовки
                messages = chatHistories[g_uiState.activeChatUser];
                
                ShowChatUI(true);
                
                // Сбрасываем скролл и перерисовываем
                if (hMessageList) {
                    InvalidateRect(hMessageList, NULL, TRUE);
                    PostMessage(hMessageList, WM_VSCROLL, SB_BOTTOM, 0);
                }
            } else {
                ShowChatUI(false);
            }
            InvalidateRect(hwnd, NULL, FALSE);
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

        // Буфер
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
        HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

        // Общий фон (темный)
        HBRUSH hBg = CreateSolidBrush(RGB(32, 34, 37)); 
        FillRect(memDC, &rect, hBg);
        DeleteObject(hBg);

        // Сайдбар с друзьями (всегда виден)
        DrawSidebarFriends(memDC, hwnd, SIDEBAR_ICONS, 0, SIDEBAR_DM, rect.bottom);

        // Центральная часть
        if (g_uiState.currentPage == AppPage::Friends) {
            DrawFriendsPage(memDC, hwnd, rect.right, rect.bottom); 
        } 
        else if (g_uiState.currentPage == AppPage::Messages) {
            // 1. Рисуем фон шапки
            HBRUSH hHeaderBr = CreateSolidBrush(RGB(49, 51, 56));
            RECT headerRect = { SIDEBAR_ICONS + SIDEBAR_DM, 0, rect.right, 48 };
            FillRect(memDC, &headerRect, hHeaderBr);
            DeleteObject(hHeaderBr);

            // 2. Рисуем текст через GDI+ (для четкости)
            Graphics gHeader(memDC);
            // ВКЛЮЧАЕМ СГЛАЖИВАНИЕ ТЕКСТА
            gHeader.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

            // УБИРАЕМ РЕШЕТКУ: Просто берем ник напрямую
            std::wstring title = Utf8ToWide(g_uiState.activeChatUser);

            FontFamily fontFamily(L"Segoe UI");
            Font headerFont(&fontFamily, 16, FontStyleBold, UnitPixel);
            SolidBrush whiteBrush(Color(255, 255, 255, 255));

            // Рисуем ник без знака #
            gHeader.DrawString(title.c_str(), -1, &headerFont, 
                            PointF((REAL)headerRect.left + 20, 14.0f), 
                            &whiteBrush);
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
            int chatX = SIDEBAR_ICONS + SIDEBAR_DM;
            int chatW = width - chatX;

            // Позиция ввода: снизу, с отступом 20 пикселей
            int inputY = height - inputEditHeight - 20;
            if (hInputEdit) {
                MoveWindow(hInputEdit, chatX + 20, inputY, chatW - 40, inputEditHeight, TRUE);
            }

            // Позиция списка: от верха (ниже хедера 48px) до ввода
            if (hMessageList) {
                MoveWindow(hMessageList, chatX, 48, chatW, inputY - 58, TRUE);
            }

            InvalidateRect(hwnd, NULL, FALSE);
            break;
        }

    case WM_CTLCOLOREDIT: {
            HDC hdcEdit = (HDC)wParam;
            if ((HWND)lParam == hInputEdit) {
                SetTextColor(hdcEdit, RGB(220, 221, 222));   
                SetBkColor(hdcEdit, RGB(56, 58, 64));       
                static HBRUSH hBrEdit = CreateSolidBrush(RGB(56, 58, 64));
                return (INT_PTR)hBrEdit;
            }
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