#include <map>
#include <string>


#include "MainPage.h"
#include "Utils/Styles.h"
#include "Components/MessageInput.h"
#include "Components/Sidebar.h"
#include "Pages/FriendsPage.h"
#include "Pages/MessagePage.h"
#include "Components/SidebarFriends.h"
#include "Components/SidebarProfile.h"
#include "Utils/Keyboard.h"
#include "Utils/Network.h"
#include "Utils/UIState.h"
#include <vector>
#include <commctrl.h>
#include <gdiplus.h>
using namespace Gdiplus; 

extern std::map<std::string, std::vector<Message>> chatHistories;
extern std::vector<Message> messages; 
extern std::string activeChatUser; 
extern std::vector<DMUser> dmUsers;
extern Gdiplus::Image* g_pMainIcon;
extern int g_activeIndex;
extern int g_hoverIndex;

HWND hMainWnd = NULL;
const int SIDEBAR_ICONS = 72;
const int SIDEBAR_DM    = 240;
static int hoveredIndex = -1;

extern int inputEditHeight;
extern HWND hInputEdit; 
extern HWND hMessageList;

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

void OpenAddMembersDialog(HWND parent) {}


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

extern std::vector<DMUser> dmUsers; 
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
    
    int oldHover = g_hoverIndex;
    if (x < SIDEBAR_ICONS) {
        if (y >= 14 && y <= 58) g_hoverIndex = 0;
        else if (y >= 80 && y <= 124) g_hoverIndex = 1;
        else if (y >= 136 && y <= 180) g_hoverIndex = 2;
        else g_hoverIndex = -1;
    } else {
        g_hoverIndex = -1;
    }

    if (oldHover != g_hoverIndex) {
        InvalidateRect(hwnd, NULL, FALSE);
    }
    break;
}

case WM_CREATE: {
            int startX = SIDEBAR_ICONS + SIDEBAR_DM;

            // ИНИЦИАЛИЗАЦИЯ ИКОНКИ САЙДБАРА
            // Так как дочернее окно удалено, загружаем ресурс в главном окне
            if (g_pMainIcon == NULL) {
                g_pMainIcon = Gdiplus::Image::FromFile(L"assets/icon.png");
                
                // Проверка: если иконка не загрузилась, вы увидите сообщение в отладчике
                if (g_pMainIcon && g_pMainIcon->GetLastStatus() != Gdiplus::Ok) {
                    OutputDebugStringA("Aegis Error: Failed to load assets/icon.png\n");
                }
            }

            hMessageList = CreateWindowExA(
                0, "MessageListWindow", NULL,
                WS_CHILD | WS_VISIBLE | WS_VSCROLL, 
                startX, 48, 100, 100, // Изменил y на 48, чтобы не перекрывать будущий хедер
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
    RECT rect;
    GetClientRect(hwnd, &rect);

    // 1. Проверка клика по профилю (САМЫЙ ВЫСОКИЙ ПРИОРИТЕТ)
    // Профиль теперь начинается от x = 0 и занимает ширину обеих колонок
    if (IsClickOnProfile(x, y, 0, rect.bottom, SIDEBAR_ICONS + SIDEBAR_DM)) {
        // Здесь логика открытия настроек профиля (если есть)
        return 0; 
    }

    // 2. Логика клика по первой колонке (Иконки серверов)
    if (x < SIDEBAR_ICONS) {
        // Проверяем координаты иконок (примерные значения из Sidebar.cpp)
        if (y >= 14 && y <= 58) g_activeIndex = 0;
        else if (y >= 80 && y <= 124) g_activeIndex = 1;
        
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    // 3. Логика клика по второй колонке (Список DM / Друзей)
    if (x >= SIDEBAR_ICONS && x <= SIDEBAR_ICONS + SIDEBAR_DM) {
        HandleSidebarFriendsClick(hwnd, x - SIDEBAR_ICONS, y);
        
        if (g_uiState.currentPage == AppPage::Messages) {
            messages = chatHistories[g_uiState.activeChatUser];
            ShowChatUI(true);
            if (hMessageList) {
                InvalidateRect(hMessageList, NULL, TRUE);
                PostMessage(hMessageList, WM_VSCROLL, SB_BOTTOM, 0);
            }
        } else {
            ShowChatUI(false);
        }
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    } 

    // 4. Логика клика по хедеру (Кнопка создания группы)
    if (g_uiState.currentPage == AppPage::Messages) {
        int btnX = rect.right - 50;
        int btnY = 9;
        if (x >= btnX && x <= btnX + 30 && y >= btnY && y <= btnY + 30) {
            OpenCreateGroupDialog(hwnd);
            return 0;
        }
    } 

    // 5. Логика клика по основному контенту (Страница друзей)
    if (g_uiState.currentPage == AppPage::Friends) {
        HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
        HandleFriendsClick(hwnd, x, y, hInstance);
        InvalidateRect(hwnd, NULL, FALSE);
    }
    
    return 0;
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

        // СЛОЙ 1: Общий фон
        HBRUSH hBg = CreateSolidBrush(RGB(32, 34, 37)); 
        FillRect(memDC, &rect, hBg);
        DeleteObject(hBg);

        // СЛОЙ 2: Первый сайдбар (Иконки серверов)
        // Рисуем функцию напрямую из Sidebar.cpp (нужно пробросить OnPaintSidebar)
        OnPaintSidebar(memDC, SIDEBAR_ICONS, rect.bottom);

        // СЛОЙ 3: Второй сайдбар (Список DM)
        DrawSidebarFriends(memDC, hwnd, SIDEBAR_ICONS, 0, SIDEBAR_DM, rect.bottom);

        // СЛОЙ 4: Основной контент (Страница или Чат)
        if (g_uiState.currentPage == AppPage::Friends) {
            DrawFriendsPage(memDC, hwnd, rect.right, rect.bottom); 
        } 
        else if (g_uiState.currentPage == AppPage::Messages) {
            // Фон хедера
            HBRUSH hHeaderBr = CreateSolidBrush(RGB(49, 51, 56));
            RECT headerRect = { SIDEBAR_ICONS + SIDEBAR_DM, 0, rect.right, 48 };
            FillRect(memDC, &headerRect, hHeaderBr);
            DeleteObject(hHeaderBr);

            Graphics gHeader(memDC);
            gHeader.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

            // Текст хедера
            std::wstring title = Utf8ToWide(g_uiState.activeChatUser);
            FontFamily fontFamily(L"Segoe UI");
            Font headerFont(&fontFamily, 16, FontStyleBold, UnitPixel);
            SolidBrush whiteBrush(Color(255, 255, 255));
            gHeader.DrawString(title.c_str(), -1, &headerFont, 
                               PointF((REAL)headerRect.left + 20, 14.0f), &whiteBrush);

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

        // СЛОЙ 5 (ВЕРХНИЙ): Панель профиля
        // Рисуем в самом конце, чтобы перекрыть НИЗ обоих сайдбаров
        {
            Graphics gUI(memDC);
            gUI.SetSmoothingMode(SmoothingModeAntiAlias);
            gUI.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

            int totalSidebarWidth = SIDEBAR_ICONS + SIDEBAR_DM;
            DrawSidebarProfile(gUI, 0, rect.bottom, totalSidebarWidth, "AdminUser");
        }

        // Вывод на экран
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

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}