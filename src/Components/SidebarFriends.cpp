#include "SidebarFriends.h"
#include "Utils/Styles.h"
#include "Utils/UIState.h"
#include <vector>
#include <mutex>
#include <algorithm>
#include <string>
#include <gdiplus.h>
#include <Utils/Utils.h>
#include <map>
#include <string>

#pragma comment(lib, "gdiplus.lib")
using namespace Gdiplus;
extern bool g_isLoadingHistory; 
extern int g_historyOffset;
extern std::vector<Message> messages; 
extern std::map<std::string, std::vector<Message>> chatHistories;
extern std::vector<DMUser> dmUsers;
extern std::mutex dmMutex; 
extern int scrollPos;
extern void ShowChatUI(bool show);
extern void RequestChatHistory(const std::string& target, int offset);
extern HWND hMessageList; 
extern UIState g_uiState;

static int hoveredIndex = -1;
const int ITEM_HEIGHT = 44;
const int ITEM_SPACING = 4;
const int TOP_OFFSET = 15;
const int BUTTON_HEIGHT = 42;
const int BUTTON_SPACING = 4;
#define COLOR_BG_DM          Color(255, 43, 45, 49)
#define COLOR_ITEM_HOVER     Color(255, 53, 55, 60)
#define COLOR_ITEM_ACTIVE    Color(255, 63, 65, 71)
#define COLOR_TEXT_BRIGHT    Color(255, 255, 255, 255)
#define COLOR_TEXT_NORMAL    Color(255, 148, 155, 164)
#define COLOR_STATUS_ONLINE  Color(255, 35, 165, 90)
#define COLOR_STATUS_OFFLINE Color(255, 128, 132, 142)

static void SortDMUsers() {
    std::sort(dmUsers.begin(), dmUsers.end(), [](const DMUser& a, const DMUser& b) {
        return a.lastMessageTime > b.lastMessageTime;
    });
}

static void DrawSmoothAvatar(Graphics& g, float x, float y, const std::string& name, bool online) {
    g.SetSmoothingMode(SmoothingModeAntiAlias);

    float size = 32.0f;
    RectF rect(x, y, size, size);
    SolidBrush avatarBrush(Color(255, 80, 84, 92));
    g.FillEllipse(&avatarBrush, rect);
    std::wstring letter = L"";
    if (!name.empty()) letter += (wchar_t)toupper(name[0]);
    FontFamily fontFamily(L"Segoe UI");
    Gdiplus::Font font(&fontFamily, 12, FontStyleBold, UnitPoint);
    SolidBrush textBrush(Color(255, 220, 221, 222));
    
    StringFormat format;
    format.SetAlignment(StringAlignmentCenter);
    format.SetLineAlignment(StringAlignmentCenter);
    g.DrawString(letter.c_str(), -1, &font, rect, &format, &textBrush);
    float sSize = 12.0f;
    float sX = x + size - sSize + 2;
    float sY = y + size - sSize + 2;
    SolidBrush bgStroke(COLOR_BG_DM);
    g.FillEllipse(&bgStroke, sX - 2, sY - 2, sSize + 4, sSize + 4);
    SolidBrush statusBrush(online ? COLOR_STATUS_ONLINE : COLOR_STATUS_OFFLINE);
    g.FillEllipse(&statusBrush, sX, sY, sSize, sSize);
}

static void DrawDiscordButton(Graphics& g, RectF rect, const std::wstring& text, bool active, bool hovered) {
    if (active || hovered) {
        SolidBrush b(active ? COLOR_ITEM_ACTIVE : COLOR_ITEM_HOVER);
        g.FillRectangle(&b, rect); 
    }

    FontFamily fontFamily(L"Segoe UI");
    Gdiplus::Font font(&fontFamily, 11, active ? FontStyleBold : FontStyleRegular, UnitPoint);
    SolidBrush textBrush(active ? COLOR_TEXT_BRIGHT : COLOR_TEXT_NORMAL);
    
    RectF textRect(rect.X + 15, rect.Y, rect.Width - 15, rect.Height);
    StringFormat format;
    format.SetLineAlignment(StringAlignmentCenter);
    g.DrawString(text.c_str(), -1, &font, textRect, &format, &textBrush);
}

void DrawSidebarFriends(HDC hdc, HWND hwnd, int x, int y, int w, int h) {
    std::lock_guard<std::mutex> lock(dmMutex);
    
    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    SolidBrush bgBrush(COLOR_BG_DM);
    g.FillRectangle(&bgBrush, (REAL)x, (REAL)y, (REAL)w, (REAL)h);
    int cy = y + TOP_OFFSET;
    const wchar_t* buttons[] = { L"Друзья", L"Запросы общения" };
    for (int i = 0; i < 2; i++) {
        RectF r((REAL)x + 8, (REAL)cy, (REAL)w - 16, (REAL)BUTTON_HEIGHT);
        bool active = (i == 0 && g_uiState.currentPage == AppPage::Friends) ||
                      (i == 1 && g_uiState.currentPage == AppPage::FriendRequests);
        
        DrawDiscordButton(g, r, buttons[i], active, (hoveredIndex == i));
        cy += BUTTON_HEIGHT + BUTTON_SPACING;
    }

    cy += 10;
    FontFamily fontFamily(L"Segoe UI");
    Gdiplus::Font titleFont(&fontFamily, 9, FontStyleBold, UnitPoint);
    SolidBrush titleBrush(COLOR_TEXT_NORMAL);
    g.DrawString(L"ЛИЧНЫЕ ЧАТЫ", -1, &titleFont, PointF((REAL)x + 18, (REAL)cy), &titleBrush);
    cy += 25; 
    for (size_t i = 0; i < dmUsers.size(); i++) {
        RectF r((REAL)x + 8, (REAL)cy, (REAL)w - 16, (REAL)ITEM_HEIGHT);
        bool active = (g_uiState.currentPage == AppPage::Messages &&
                       g_uiState.activeChatUser == dmUsers[i].username);
        if (active || hoveredIndex == (int)i + 2) {
            SolidBrush b(active ? COLOR_ITEM_ACTIVE : COLOR_ITEM_HOVER);
            g.FillRectangle(&b, r);
        }

        DrawSmoothAvatar(g, r.X + 10, r.Y + 6, dmUsers[i].username, dmUsers[i].online);
        Gdiplus::Font nameFont(&fontFamily, 11, active ? FontStyleBold : FontStyleRegular, UnitPoint);
        SolidBrush nameBrush(active ? COLOR_TEXT_BRIGHT : COLOR_TEXT_NORMAL);
        RectF textRect(r.X + 50, r.Y, r.Width - 50, r.Height);
        StringFormat format;
        format.SetLineAlignment(StringAlignmentCenter);
        g.DrawString(std::wstring(dmUsers[i].username.begin(), dmUsers[i].username.end()).c_str(), 
                     -1, &nameFont, textRect, &format, &nameBrush);

        cy += ITEM_HEIGHT + ITEM_SPACING;
    }
}

void HandleSidebarFriendsClick(HWND hwnd, int x, int y) {
    std::lock_guard<std::mutex> lock(dmMutex);
    int cy = TOP_OFFSET;
    if (y >= cy && y <= cy + BUTTON_HEIGHT) {
        g_uiState.currentPage = AppPage::Friends;
        ShowChatUI(false);
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }
    cy += BUTTON_HEIGHT + BUTTON_SPACING;
    if (y >= cy && y <= cy + BUTTON_HEIGHT) {
        g_uiState.currentPage = AppPage::FriendRequests;
        ShowChatUI(false);
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }
    
    cy += BUTTON_HEIGHT + BUTTON_SPACING + 10 + 25; 
    for (auto& u : dmUsers) {
            if (y >= cy && y <= cy + ITEM_HEIGHT) {
                if (g_uiState.activeChatUser == u.username) return;
                g_uiState.activeChatUser = u.username;
                g_uiState.currentPage = AppPage::Messages;
                messages.clear();
                if (chatHistories.count(u.username)) {
                    messages = chatHistories[u.username];
                }
                g_historyOffset = 0;
                g_isLoadingHistory = false; 
                RequestChatHistory(u.username, 0); 
                ShowChatUI(true);
                scrollPos = 0; 
                InvalidateRect(hwnd, NULL, FALSE);
                return;
            }
        cy += ITEM_HEIGHT + ITEM_SPACING;
    }
}
void AddUserToDMList(HWND hwnd, const std::string& username, bool isOnline) {
    std::lock_guard<std::mutex> lock(dmMutex);
    auto it = std::find_if(dmUsers.begin(), dmUsers.end(), [&](const DMUser& u) {
        return u.username == username;
    });
    if (it == dmUsers.end()) {
        DMUser newUser;
        newUser.username = username;
        newUser.online = isOnline; 
        newUser.lastMessageTime = 0; 
        dmUsers.push_back(newUser);
        SortDMUsers();
    } else {
        it->online = isOnline; 
    }
    if (hwnd) InvalidateRect(hwnd, NULL, FALSE);
}
void UpdateUserActivity(const std::string& username) {
    std::lock_guard<std::mutex> lock(dmMutex);
    
    auto it = std::find_if(dmUsers.begin(), dmUsers.end(), [&](const DMUser& u) {
        return u.username == username;
    });

    if (it != dmUsers.end()) {
        it->lastMessageTime = (long long)time(NULL);
        SortDMUsers(); 
    }
}
void UpdateUserOnlineStatus(const std::string& username, bool isOnline) {
    std::lock_guard<std::mutex> lock(dmMutex);
    
    auto it = std::find_if(dmUsers.begin(), dmUsers.end(), [&](const DMUser& u) {
        return u.username == username;
    });

    if (it != dmUsers.end()) {
        it->online = isOnline;
    }
}
