#include "SidebarFriends.h"
#include "Utils/Styles.h"
#include "Utils/UIState.h"
#include <vector>
#include <mutex>
#include <algorithm>

// Внешние функции
extern void ShowChatUI(bool show); 

// Глобальные переменные модуля с защитой
static std::vector<DMUser> dmUsers;
static std::mutex dmMutex; // Для защиты вектора при работе с сетью
static int hoveredIndex = -1;

#define COLOR_BG_DM         RGB(47, 49, 54)
#define COLOR_ITEM_HOVER    RGB(64, 68, 75)
#define COLOR_ITEM_ACTIVE   RGB(88, 101, 242)
#define COLOR_TEXT          RGB(220, 221, 222)
#define COLOR_MUTED         RGB(150, 152, 157)

const int SIDEBAR_ICONS = 72;
const int SIDEBAR_DM    = 240;

static void FillRectSafe(HDC hdc, RECT* r, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(hdc, r, brush);
    DeleteObject(brush);
}

void SetDMUsers(const std::vector<DMUser>& users) {
    std::lock_guard<std::mutex> lock(dmMutex);
    dmUsers = users;
}

void DrawSidebarFriends(HDC hdc, HWND hwnd, int x, int y, int w, int h) {
    std::lock_guard<std::mutex> lock(dmMutex); // Блокируем на время отрисовки

    RECT bg = { x, y, x + w, y + h };
    FillRectSafe(hdc, &bg, COLOR_BG_DM);

    HFONT fontTitle = CreateAppFont(12, FW_BOLD);
    HFONT fontItem  = CreateAppFont(13, FW_NORMAL);
    HFONT oldFont   = (HFONT)SelectObject(hdc, fontItem);

    SetBkMode(hdc, TRANSPARENT);
    int cy = y + 12;

    // --- Кнопки управления ---
    const wchar_t* buttons[] = { L"Друзья", L"Запросы общения" };
    for (int i = 0; i < 2; i++) {
        RECT r = { x + 8, cy, x + w - 8, cy + 36 };
        bool active = false;
        if (i == 0) active = (g_uiState.currentPage == AppPage::Friends);
        if (i == 1) active = (g_uiState.currentPage == AppPage::FriendRequests);
        
        if (active) {
            FillRectSafe(hdc, &r, COLOR_ITEM_HOVER);
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

    // Заголовок
    SelectObject(hdc, fontTitle);
    RECT titleRect = { x + 12, cy, x + w, cy + 24 };
    SetTextColor(hdc, COLOR_MUTED);
    DrawTextW(hdc, L"ЛИЧНЫЕ ЧАТЫ", -1, &titleRect, DT_LEFT | DT_VCENTER);
    cy += 28;

    // --- Список пользователей ---
    SelectObject(hdc, fontItem);
    for (size_t i = 0; i < dmUsers.size(); i++) {
        RECT r = { x + 8, cy, x + w - 8, cy + 36 };
        bool active = (g_uiState.currentPage == AppPage::Messages && 
                       g_uiState.activeChatUser == dmUsers[i].username);

        if (active || hoveredIndex == (int)i + 2)
            FillRectSafe(hdc, &r, active ? COLOR_ITEM_ACTIVE : COLOR_ITEM_HOVER);

        SetTextColor(hdc, active ? RGB(255, 255, 255) : COLOR_TEXT);
        DrawTextA(hdc, dmUsers[i].username.c_str(), -1, &r, DT_VCENTER | DT_SINGLELINE | DT_LEFT | DT_NOPREFIX);
        cy += 40;
    }

    SelectObject(hdc, oldFont);
    DeleteObject(fontTitle);
    DeleteObject(fontItem);
}

void HandleSidebarFriendsClick(HWND hwnd, int x, int y) {
    std::lock_guard<std::mutex> lock(dmMutex);
    int cy = 12;

    if (y >= cy && y <= cy + 36) { 
        g_uiState.currentPage = AppPage::Friends;
        ShowChatUI(false);
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }
    cy += 40;

    if (y >= cy && y <= cy + 36) { 
        g_uiState.currentPage = AppPage::FriendRequests;
        ShowChatUI(false);
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }
    cy += 64;

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

void AddUserToDMList(HWND hwnd, const std::string& username) {
    std::lock_guard<std::mutex> lock(dmMutex);
    
    auto it = std::find_if(dmUsers.begin(), dmUsers.end(), [&](const DMUser& u) {
        return u.username == username;
    });
    if (it != dmUsers.end()) return; 

    DMUser newUser;
    newUser.username = username;
    newUser.id = "room_id"; 
    dmUsers.push_back(newUser);

    if (hwnd) {
        RECT r = { 72, 0, 72 + 240, 2000 };
        InvalidateRect(hwnd, &r, FALSE);
    }
}

void UpdateAllFriends(const std::vector<DMUser>& friends) {
    std::lock_guard<std::mutex> lock(dmMutex);
    dmUsers = friends;
}