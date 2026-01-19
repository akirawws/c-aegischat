// =============================================================
// ВАЖНО: winsock2.h должен быть первым!
// =============================================================
#include <winsock2.h> 
#include <windows.h>
#include <map>
#include <string>
#include <vector>
#include <commctrl.h>
#include <gdiplus.h>

// =============================================================
// ПОДКЛЮЧЕНИЕ ЗАГОЛОВКОВ ПРОЕКТА
// =============================================================
#include "MainPage.h"
#include "Utils/Styles.h"
#include "Utils/Utils.h"
#include "Utils/Keyboard.h"
#include "Utils/Network.h"
#include "Utils/UIState.h"
#include "Utils/Messages.h"
#include "Components/MessageInput.h"
#include "Components/Sidebar.h"
#include "Components/SidebarFriends.h"
#include "Components/SidebarProfile.h"
#include "Components/MessageList.h"
#include "Components/Settings.h" 
#include "Pages/FriendsPage.h"
#include "Pages/MessagePage.h"
// ВАЖНО: Подключаем заголовок блога
#include "DeveloperBlog.h" 

using namespace Gdiplus; 

// =============================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
// =============================================================
extern std::map<std::string, ChatCache> chatHistories;
extern std::vector<Message> messages; 
extern std::string activeChatUser; 
extern std::vector<DMUser> dmUsers;
extern Gdiplus::Image* g_pMainIcon;
extern int g_activeIndex;
extern int g_hoverIndex;
extern int inputEditHeight;
extern HWND hInputEdit; 
extern HWND hMessageList;

// Переменные для скролла
int g_blogScroll = 0;           // Скролл для блога
int g_scrollOffset = 0;         // Скролл для чата
int g_totalMessageHeight = 0;   
const int MESSAGE_HEIGHT = 24; 

// Константы
const int SIDEBAR_ICONS = 72;
const int SIDEBAR_DM    = 240;
#define SIDEBAR_PROFILE_SETTINGS 998

// Локальные переменные
HWND hMainWnd = NULL;
static int hoveredIndex = -1;

// =============================================================
// ОБЪЯВЛЕНИЯ ФУНКЦИЙ
// =============================================================
bool IsClickOnSettingsIcon(int x, int y, int sidebarX, int windowHeight, int totalWidth);
void OpenAddMembersDialog(HWND parent);
void RequestCreateGroup(const std::vector<std::string>& members); // Убедитесь, что эта функция объявлена в Network.h

// =============================================================
// ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
// =============================================================

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

void CenterWindow(HWND hwnd, HWND hwndParent) {
    RECT rect, rectP;
    GetWindowRect(hwnd, &rect);
    GetWindowRect(hwndParent, &rectP);

    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    int parentWidth = rectP.right - rectP.left;
    int parentHeight = rectP.bottom - rectP.top;
    int x = rectP.left + (parentWidth - width) / 2;
    int y = rectP.top + (parentHeight - height) / 2;

    SetWindowPos(hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

void ShowChatUI(bool show) {
    int cmd = show ? SW_SHOW : SW_HIDE;
    if (hInputEdit) ShowWindow(hInputEdit, cmd);
    if (hMessageList) ShowWindow(hMessageList, cmd); 
}

// =============================================================
// ДИАЛОГ СОЗДАНИЯ ГРУППЫ
// =============================================================
LRESULT CALLBACK CreateGroupDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CTLCOLORLISTBOX: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, RGB(220, 221, 222));
        SetBkColor(hdc, RGB(43, 45, 49)); 
        static HBRUSH hBr = CreateSolidBrush(RGB(43, 45, 49));
        return (INT_PTR)hBr;
    }
    case WM_COMMAND: {
        if (LOWORD(wParam) == 2002) {
            HWND hList = GetDlgItem(hwnd, 2001);
            int count = SendMessage(hList, LB_GETSELCOUNT, 0, 0);
            if (count > 0) {
                std::vector<int> selections(count);
                SendMessage(hList, LB_GETSELITEMS, count, (LPARAM)selections.data());
                
                std::vector<std::string> selectedMembers;
                for (int idx : selections) {
                    wchar_t buffer[256];
                    SendMessageW(hList, LB_GETTEXT, idx, (LPARAM)buffer);
                    selectedMembers.push_back(WideToUtf8(buffer)); 
                }
                
                RequestCreateGroup(selectedMembers);
                SendMessage(hwnd, WM_CLOSE, 0, 0);
            } else {
                MessageBoxW(hwnd, L"Выберите друзей!", L"AEGIS", MB_OK | MB_ICONWARNING);
            }
        }
        break;
    }
    case WM_CLOSE:
        EnableWindow(hMainWnd, TRUE);
        DestroyWindow(hwnd);
        break;
    default:
        return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

void OpenCreateGroupDialog(HWND parent) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSW wc = {0};
        wc.lpfnWndProc = CreateGroupDlgProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.hbrBackground = CreateSolidBrush(RGB(49, 51, 56)); 
        wc.lpszClassName = L"CreateGroupClass";
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        RegisterClassW(&wc);
        registered = true;
    }

    HWND hDlg = CreateWindowExW(
        WS_EX_TOPMOST, L"CreateGroupClass", L"Создать беседу",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 320, 480,
        parent, NULL, GetModuleHandle(NULL), NULL
    );
    HWND hList = CreateWindowExW(
        0, L"ListBox", NULL,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_MULTIPLESEL | LBS_HASSTRINGS,
        20, 20, 265, 340,
        hDlg, (HMENU)2001, GetModuleHandle(NULL), NULL
    );

    HFONT hFont = CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    SendMessage(hList, WM_SETFONT, (WPARAM)hFont, TRUE);
    for (const auto& user : dmUsers) {
        std::wstring wname = Utf8ToWide(user.username);
        SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)wname.c_str());
    }

    HWND btn = CreateWindowExW(
        0, L"Button", L"Создать группу",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        20, 380, 265, 40,
        hDlg, (HMENU)2002, GetModuleHandle(NULL), NULL
    );
    SendMessage(btn, WM_SETFONT, (WPARAM)hFont, TRUE);

    CenterWindow(hDlg, parent);
    ShowWindow(hDlg, SW_SHOW);
    EnableWindow(parent, FALSE); 
}

// =============================================================
// ГЛАВНАЯ ПРОЦЕДУРА ОКНА (MainWndProc)
// =============================================================
LRESULT CALLBACK MainWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {

    case WM_CREATE: {
        int startX = SIDEBAR_ICONS + SIDEBAR_DM;

        // Загрузка иконки
        if (g_pMainIcon == NULL) {
            g_pMainIcon = Gdiplus::Image::FromFile(L"assets/icon.png");
            if (g_pMainIcon && g_pMainIcon->GetLastStatus() != Gdiplus::Ok) {
                OutputDebugStringA("Aegis Error: Failed to load assets/icon.png\n");
            }
        }

        // Окно списка сообщений (изначально скрыто)
        hMessageList = CreateWindowExA(
            0, "MessageListWindow", NULL,
            WS_CHILD | WS_CLIPSIBLINGS | WS_VSCROLL, 
            startX, 48, 100, 100,
            hwnd, NULL, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL
        );

        // Поле ввода (изначально скрыто)
        CreateMessageInput(hwnd, startX + 10, 100, 100, INPUT_MIN_HEIGHT);
        if (hInputEdit) {
            SetWindowSubclass(hInputEdit, MessageInputSubclass, 0, (DWORD_PTR)hwnd);
        }

        g_uiState.currentPage = AppPage::Friends; // Стартовая страница
        g_uiState.activeChatUser = "";
        
        ShowChatUI(false); 
        return 0;
    }

    case WM_COMMAND: {
        if (LOWORD(wParam) == 1001) {
            SendPrivateMessageFromUI();
        }
        else if (LOWORD(wParam) == 2002) {
            HWND hDlg = GetParent((HWND)lParam);
            HWND hList = GetDlgItem(hDlg, 2001);
            int count = SendMessage(hList, LB_GETSELCOUNT, 0, 0);
            if (count > 0) {
                std::vector<int> selections(count);
                SendMessage(hList, LB_GETSELITEMS, count, (LPARAM)selections.data());
                std::vector<std::string> selectedMembers;
                for (int idx : selections) {
                    char buffer[64];
                    SendMessageA(hList, LB_GETTEXT, idx, (LPARAM)buffer);
                    selectedMembers.push_back(buffer);
                }
                RequestCreateGroup(selectedMembers); 
                EnableWindow(hMainWnd, TRUE); 
                DestroyWindow(hDlg);
            } else {
                MessageBoxA(hDlg, "Выберите хотя бы одного друга!", "Ошибка", MB_OK | MB_ICONWARNING);
            }
        }
        break;
    }

    case WM_MOUSEMOVE: {
        int x = LOWORD(lParam);
        int y = HIWORD(lParam);
        RECT clientRect;
        GetClientRect(hwnd, &clientRect);

        int oldHover = g_hoverIndex;
        g_hoverIndex = -1;

        if (x < SIDEBAR_ICONS) {
            if (y >= 14 && y <= 58) g_hoverIndex = 0;
            else if (y >= 80 && y <= 124) g_hoverIndex = 1;
        }

        if (IsClickOnSettingsIcon(x, y, 0, clientRect.bottom, SIDEBAR_ICONS + SIDEBAR_DM)) {
            g_hoverIndex = SIDEBAR_PROFILE_SETTINGS;
        }

        if (oldHover != g_hoverIndex) {
            InvalidateRect(hwnd, NULL, FALSE);
        }
        break;
    }

    case WM_LBUTTONDOWN: {
        int x = LOWORD(lParam);
        int y = HIWORD(lParam);
        RECT rect;
        GetClientRect(hwnd, &rect);

        // 1. Настройки
        if (IsClickOnSettingsIcon(x, y, 0, rect.bottom, SIDEBAR_ICONS + SIDEBAR_DM)) {
            OpenSettingsDialog(hwnd);
            return 0;
        }

        // 2. Профиль
        if (IsClickOnProfile(x, y, 0, rect.bottom, SIDEBAR_ICONS + SIDEBAR_DM)) {
            return 0; 
        }

        // 3. ПЕРВАЯ КОЛОНКА (Сайдбар иконок)
        if (x < SIDEBAR_ICONS) {
            if (y >= 14 && y <= 58) {
                // Клик по "Главной" (Discord Icon)
                g_activeIndex = 0;
                g_uiState.currentPage = AppPage::Friends;
                ShowChatUI(false);
            }
            else if (y >= 80 && y <= 124) {
                // Клик по "Dev Blog"
                g_activeIndex = 1;
                g_uiState.currentPage = AppPage::DevBlog;
                g_blogScroll = 0; // Сбрасываем скролл
                ShowChatUI(false);
            }
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }

        // 4. ВТОРАЯ КОЛОНКА (Список друзей / Чатов)
        if (x >= SIDEBAR_ICONS && x <= SIDEBAR_ICONS + SIDEBAR_DM) {
            HandleSidebarFriendsClick(hwnd, x - SIDEBAR_ICONS, y);
            
            if (g_uiState.currentPage == AppPage::Messages) {
                messages = chatHistories[g_uiState.activeChatUser].messages;
                ShowChatUI(true);
                if (hMessageList) {
                    InvalidateRect(hMessageList, NULL, TRUE);
                    ScrollMessagesToBottom();
                }
            } else {
                ShowChatUI(false);
            }
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        } 

        // 5. ОСНОВНОЙ КОНТЕНТ (Кнопки внутри страниц)
        if (g_uiState.currentPage == AppPage::Messages) {
            int btnX = rect.right - 50;
            int btnY = 9;
            if (x >= btnX && x <= btnX + 30 && y >= btnY && y <= btnY + 30) {
                OpenCreateGroupDialog(hwnd);
                return 0;
            }
        } 
        else if (g_uiState.currentPage == AppPage::Friends) {
            HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
            HandleFriendsClick(hwnd, x, y, hInstance);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_MOUSEWHEEL: {
        short delta = GET_WHEEL_DELTA_WPARAM(wParam);
        
        if (g_uiState.currentPage == AppPage::DevBlog) {
            // --- СКРОЛЛ ДЛЯ БЛОГА ---
            int scrollSpeed = delta / 2; 
            g_blogScroll -= scrollSpeed;

            RECT r; GetClientRect(hwnd, &r);
            extern int g_totalBlogHeight; // Из DeveloperBlog.cpp
            int maxScroll = g_totalBlogHeight - r.bottom + 50; 
            if (maxScroll < 0) maxScroll = 0;

            if (g_blogScroll < 0) g_blogScroll = 0;
            if (g_blogScroll > maxScroll) g_blogScroll = maxScroll;

            InvalidateRect(hwnd, NULL, FALSE);
        }
        else if (g_uiState.currentPage == AppPage::Messages) {
            // --- СКРОЛЛ ДЛЯ ЧАТА ---
            g_scrollOffset -= delta;
            g_scrollOffset = std::max(0, std::min(g_scrollOffset, std::max(0, g_totalMessageHeight - (HIWORD(lParam) - LOWORD(lParam)))));
            if (hMessageList) InvalidateRect(hMessageList, NULL, TRUE);
        }
        break;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rect;
        GetClientRect(hwnd, &rect);

        // Двойная буферизация
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
        HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

        // Фон
        HBRUSH hBg = CreateSolidBrush(RGB(32, 34, 37)); 
        FillRect(memDC, &rect, hBg);
        DeleteObject(hBg);

        // Сайдбары
        OnPaintSidebar(memDC, SIDEBAR_ICONS, rect.bottom);
        DrawSidebarFriends(memDC, hwnd, SIDEBAR_ICONS, 0, SIDEBAR_DM, rect.bottom);

        // Контент
        if (g_uiState.currentPage == AppPage::Friends) {
            DrawFriendsPage(memDC, hwnd, rect.right, rect.bottom); 
        } 
        else if (g_uiState.currentPage == AppPage::DevBlog) {
            DrawDeveloperBlogPage(memDC, rect, SIDEBAR_ICONS + SIDEBAR_DM, g_blogScroll);
        }
        else if (g_uiState.currentPage == AppPage::Messages) {
            // Хедер чата
            HBRUSH hHeaderBr = CreateSolidBrush(RGB(49, 51, 56));
            RECT headerRect = { SIDEBAR_ICONS + SIDEBAR_DM, 0, rect.right, 48 };
            FillRect(memDC, &headerRect, hHeaderBr);
            DeleteObject(hHeaderBr);

            Graphics gHeader(memDC);
            gHeader.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

            std::wstring title = Utf8ToWide(g_uiState.activeChatUser);
            FontFamily fontFamily(L"Segoe UI");
            Font headerFont(&fontFamily, 16, FontStyleBold, UnitPixel);
            SolidBrush whiteBrush(Color(255, 255, 255));
            gHeader.DrawString(title.c_str(), -1, &headerFont, PointF((REAL)headerRect.left + 20, 14.0f), &whiteBrush);

            // Кнопка "+"
            RectF btnRect((REAL)rect.right - 50, 9.0f, 30.0f, 30.0f);
            SolidBrush btnBrush((hoveredIndex == 999) ? Color(255, 78, 80, 88) : Color(255, 59, 61, 68));
            gHeader.FillEllipse(&btnBrush, btnRect);
            
            Font plusFont(&fontFamily, 18, FontStyleRegular, UnitPixel);
            StringFormat sf;
            sf.SetAlignment(StringAlignmentCenter);
            sf.SetLineAlignment(StringAlignmentCenter);
            gHeader.DrawString(L"+", -1, &plusFont, btnRect, &sf, &whiteBrush);
        }

        // Профиль (поверх всего)
        {
            Graphics gUI(memDC);
            gUI.SetSmoothingMode(SmoothingModeAntiAlias);
            gUI.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
            int totalSidebarWidth = SIDEBAR_ICONS + SIDEBAR_DM;
            DrawSidebarProfile(gUI, 0, rect.bottom, totalSidebarWidth, "AdminUser");
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
        int inputY = height - inputEditHeight - 20;

        if (hInputEdit) {
            MoveWindow(hInputEdit, chatX + 20, inputY, chatW - 40, inputEditHeight, TRUE);
        }
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

    case WM_USER + 200: {
        EnableWindow(hwnd, TRUE);
        SetFocus(hwnd);
        InvalidateRect(hwnd, NULL, TRUE);
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