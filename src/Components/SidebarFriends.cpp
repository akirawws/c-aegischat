#include "SidebarFriends.h"
#include "Utils/Styles.h"
#include "Utils/UIState.h"
#include <vector>

// Предполагаем, что эти функции объявлены в MainPage.h или внешние
extern void ShowChatUI(bool show); 

static std::vector<DMUser> dmUsers;
static int hoveredIndex = -1;

#define COLOR_BG_DM        RGB(47, 49, 54)
#define COLOR_ITEM_HOVER   RGB(64, 68, 75)
#define COLOR_ITEM_ACTIVE  RGB(88, 101, 242)
#define COLOR_TEXT         RGB(220, 221, 222)
#define COLOR_MUTED        RGB(150, 152, 157)

const int SIDEBAR_ICONS = 72;
const int SIDEBAR_DM    = 240;

void SetDMUsers(const std::vector<DMUser>& users) {
    dmUsers = users;
}

static void FillRectSafe(HDC hdc, RECT* r, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(hdc, r, brush);
    DeleteObject(brush);
}

void DrawSidebarFriends(HDC hdc, HWND hwnd, int x, int y, int w, int h) {
    RECT bg = { x, y, x + w, y + h };
    FillRectSafe(hdc, &bg, COLOR_BG_DM);

    HFONT fontTitle = CreateAppFont(12, FW_BOLD);
    HFONT fontItem  = CreateAppFont(13, FW_NORMAL);
    HFONT oldFont   = (HFONT)SelectObject(hdc, fontItem);

    SetBkMode(hdc, TRANSPARENT);
    int cy = y + 12;

    // === Кнопки управления (Друзья / Запросы) ===
    const wchar_t* buttons[] = { L"Друзья", L"Запросы общения" };
    for (int i = 0; i < 2; i++) {
        RECT r = { x + 8, cy, x + w - 8, cy + 36 };

        // Логика подсветки активной кнопки
        bool active = false;
        if (i == 0) active = (g_uiState.currentPage == AppPage::Friends);
        if (i == 1) active = (g_uiState.currentPage == AppPage::FriendRequests);
        
        if (active) {
            FillRectSafe(hdc, &r, COLOR_ITEM_HOVER); // Или COLOR_ITEM_ACTIVE
            SetTextColor(hdc, RGB(255, 255, 255));
        } else if (hoveredIndex == i) {
            FillRectSafe(hdc, &r, COLOR_ITEM_HOVER);
            SetTextColor(hdc, COLOR_TEXT);
        } else {
            SetTextColor(hdc, COLOR_MUTED);
        }

        DrawTextW(hdc, buttons[i], -1, &r, DT_VCENTER | DT_SINGLELINE | DT_LEFT | DT_NOPREFIX);
        cy += 40;
    }

    // Разделитель
    RECT sep = { x + 12, cy + 8, x + w - 12, cy + 9 };
    FillRectSafe(hdc, &sep, RGB(60, 63, 69));
    cy += 24;

    // Заголовок ЛС
    SelectObject(hdc, fontTitle);
    RECT title = { x + 12, cy, x + w, cy + 24 };
    SetTextColor(hdc, COLOR_MUTED);
    DrawTextW(hdc, L"ЛИЧНЫЕ ЧАТЫ", -1, &title, DT_LEFT | DT_VCENTER);
    cy += 28;

    // Список пользователей
    SelectObject(hdc, fontItem);
    for (size_t i = 0; i < dmUsers.size(); i++) {
        RECT r = { x + 8, cy, x + w - 8, cy + 36 };

        bool active = (g_uiState.currentPage == AppPage::Messages && 
                       g_uiState.activeChatUser == dmUsers[i].username);

        if (active || hoveredIndex == (int)i + 2)
            FillRectSafe(hdc, &r, active ? COLOR_ITEM_ACTIVE : COLOR_ITEM_HOVER);

        SetTextColor(hdc, (active) ? RGB(255, 255, 255) : COLOR_TEXT);
        DrawTextA(hdc, dmUsers[i].username.c_str(), -1, &r, DT_VCENTER | DT_SINGLELINE | DT_LEFT | DT_NOPREFIX);
        cy += 40;
    }

    SelectObject(hdc, oldFont);
    DeleteObject(fontTitle);
    DeleteObject(fontItem);
}

void HandleSidebarFriendsHover(HWND hwnd, int x, int y) {
    int oldHover = hoveredIndex;
    hoveredIndex = -1;

    // УБРАТЬ проверку X - она уже выполнена в MainWndProc
    // if (x < SIDEBAR_ICONS || x > SIDEBAR_ICONS + SIDEBAR_DM) {
    //     if (oldHover != -1) InvalidateRect(hwnd, NULL, FALSE);
    //     return;
    // }

    int cy = 12;
    if (y >= cy && y <= cy + 36) hoveredIndex = 0;
    cy += 40;
    if (y >= cy && y <= cy + 36) hoveredIndex = 1;
    cy += 64;

    for (size_t i = 0; i < dmUsers.size(); i++) {
        if (y >= cy && y <= cy + 36) {
            hoveredIndex = (int)i + 2;
            break;
        }
        cy += 40;
    }

    if (oldHover != hoveredIndex) {
        RECT r = { SIDEBAR_ICONS, 0, SIDEBAR_ICONS + SIDEBAR_DM, 2000 };
        InvalidateRect(hwnd, &r, FALSE);
    }
}

void HandleSidebarFriendsClick(HWND hwnd, int x, int y) {
    // УБРАТЬ проверку X - она уже выполнена в MainWndProc
    // if (x < SIDEBAR_ICONS || x > SIDEBAR_ICONS + SIDEBAR_DM) return;

    int cy = 12;

    // Клик по "Друзья"
    if (y >= cy && y <= cy + 36) { 
        g_uiState.currentPage = AppPage::Friends;
        ShowChatUI(false);
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }
    cy += 40;

    // Клик по "Запросы"
    if (y >= cy && y <= cy + 36) { 
        g_uiState.currentPage = AppPage::FriendRequests;
        ShowChatUI(false);
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }
    cy += 64;

    // Клик по пользователю
    for (auto& u : dmUsers) {
        if (y >= cy && y <= cy + 36) {
            g_uiState.currentPage = AppPage::Messages;
            g_uiState.activeChatUser = u.username;
            ShowChatUI(true);
            InvalidateRect(hwnd, NULL, FALSE);
            return;
        }
        cy += 40;
    }
}