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
#include <filesystem>

#pragma comment(lib, "gdiplus.lib")

namespace fs = std::filesystem;

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
extern std::string g_cacheDir; 
extern void ScrollMessagesToBottom();


static int hoveredIndex = -1;
const int ITEM_HEIGHT = 44;
const int ITEM_SPACING = 4;
const int TOP_OFFSET = 15;
const int BUTTON_HEIGHT = 42;
const int BUTTON_SPACING = 4;

#define COLOR_BG_DM          Gdiplus::Color(255, 43, 45, 49)
#define COLOR_ITEM_HOVER     Gdiplus::Color(255, 53, 55, 60)
#define COLOR_ITEM_ACTIVE    Gdiplus::Color(255, 63, 65, 71)
#define COLOR_TEXT_BRIGHT    Gdiplus::Color(255, 255, 255, 255)
#define COLOR_TEXT_NORMAL    Gdiplus::Color(255, 148, 155, 164)
#define COLOR_STATUS_ONLINE  Gdiplus::Color(255, 35, 165, 90)
#define COLOR_STATUS_OFFLINE Gdiplus::Color(255, 128, 132, 142)

static void SortDMUsers() {
    std::sort(dmUsers.begin(), dmUsers.end(), [](const DMUser& a, const DMUser& b) {
        return a.lastMessageTime > b.lastMessageTime;
    });
}

static void DrawSmoothAvatar(Gdiplus::Graphics& g, float x, float y, const std::string& name, bool online) {
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    float size = 32.0f;
    Gdiplus::RectF rect(x, y, size, size);

    std::string avatarPath = GetUserAvatarPath(name);
    if (!avatarPath.empty() && fs::exists(avatarPath)) {
        std::wstring avatarPathW(avatarPath.begin(), avatarPath.end());
        Gdiplus::Image* img = Gdiplus::Image::FromFile(avatarPathW.c_str());
        if (img && img->GetLastStatus() == Gdiplus::Ok) {
            Gdiplus::GraphicsPath path;
            path.AddEllipse(rect);
            Gdiplus::Region clipRegion(&path);
            Gdiplus::GraphicsState state = g.Save();
            g.SetClip(&clipRegion, Gdiplus::CombineModeReplace);
            g.DrawImage(img, rect);
            g.Restore(state);
            delete img;
            goto draw_status; // пропускаем заглушку
        }
        delete img;
    }

    // Заглушка
    {
        Gdiplus::SolidBrush avatarBrush(Gdiplus::Color(255, 80, 84, 92));
        g.FillEllipse(&avatarBrush, rect);
        if (!name.empty()) {
            wchar_t firstChar = (wchar_t)(unsigned char)std::toupper((unsigned char)name[0]);
            std::wstring letter{firstChar};
            Gdiplus::FontFamily fontFamily(L"Segoe UI");
            Gdiplus::Font font(&fontFamily, 12, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
            Gdiplus::SolidBrush textBrush(Gdiplus::Color(255, 220, 221, 222));
            Gdiplus::StringFormat format;
            format.SetAlignment(Gdiplus::StringAlignmentCenter);
            format.SetLineAlignment(Gdiplus::StringAlignmentCenter);
            g.DrawString(letter.c_str(), -1, &font, rect, &format, &textBrush);
        }
    }

draw_status:
    float sSize = 12.0f;
    float sX = x + size - sSize + 2;
    float sY = y + size - sSize + 2;
    Gdiplus::SolidBrush bgStroke(COLOR_BG_DM);
    g.FillEllipse(&bgStroke, sX - 2, sY - 2, sSize + 4, sSize + 4);
    Gdiplus::SolidBrush statusBrush(online ? COLOR_STATUS_ONLINE : COLOR_STATUS_OFFLINE);
    g.FillEllipse(&statusBrush, sX, sY, sSize, sSize);
}

static void DrawDiscordButton(Gdiplus::Graphics& g, Gdiplus::RectF rect, const std::wstring& text, bool active, bool hovered) {
    if (active || hovered) {
        Gdiplus::SolidBrush b(active ? COLOR_ITEM_ACTIVE : COLOR_ITEM_HOVER);
        g.FillRectangle(&b, rect); 
    }

    Gdiplus::FontFamily fontFamily(L"Segoe UI");
    Gdiplus::Font font(&fontFamily, 11, active ? Gdiplus::FontStyleBold : Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
    Gdiplus::SolidBrush textBrush(active ? COLOR_TEXT_BRIGHT : COLOR_TEXT_NORMAL);
    
    Gdiplus::RectF textRect(rect.X + 15, rect.Y, rect.Width - 15, rect.Height);
    Gdiplus::StringFormat format;
    format.SetLineAlignment(Gdiplus::StringAlignmentCenter);
    g.DrawString(text.c_str(), -1, &font, textRect, &format, &textBrush);
}

void DrawSidebarFriends(HDC hdc, HWND hwnd, int x, int y, int w, int h) {
    std::lock_guard<std::mutex> lock(dmMutex);
    
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
    Gdiplus::SolidBrush bgBrush(COLOR_BG_DM);
    g.FillRectangle(&bgBrush, (Gdiplus::REAL)x, (Gdiplus::REAL)y, (Gdiplus::REAL)w, (Gdiplus::REAL)h);

    int cy = y + TOP_OFFSET;
    const wchar_t* buttons[] = { L"Друзья", L"Запросы общения" };
    for (int i = 0; i < 2; i++) {
        Gdiplus::RectF r((Gdiplus::REAL)x + 8, (Gdiplus::REAL)cy, (Gdiplus::REAL)w - 16, (Gdiplus::REAL)BUTTON_HEIGHT);
        bool active = (i == 0 && g_uiState.currentPage == AppPage::Friends) ||
                      (i == 1 && g_uiState.currentPage == AppPage::FriendRequests);
        
        DrawDiscordButton(g, r, buttons[i], active, (hoveredIndex == i));
        cy += BUTTON_HEIGHT + BUTTON_SPACING;
    }

    cy += 10;
    Gdiplus::FontFamily fontFamily(L"Segoe UI");
    Gdiplus::Font titleFont(&fontFamily, 9, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
    Gdiplus::SolidBrush titleBrush(COLOR_TEXT_NORMAL);
    g.DrawString(L"ЛИЧНЫЕ ЧАТЫ", -1, &titleFont, Gdiplus::PointF((Gdiplus::REAL)x + 18, (Gdiplus::REAL)cy), &titleBrush);
    cy += 25; 

    for (size_t i = 0; i < dmUsers.size(); i++) {
        Gdiplus::RectF r((Gdiplus::REAL)x + 8, (Gdiplus::REAL)cy, (Gdiplus::REAL)w - 16, (Gdiplus::REAL)ITEM_HEIGHT);
        bool active = (g_uiState.currentPage == AppPage::Messages &&
                       g_uiState.activeChatUser == dmUsers[i].username);
        if (active || hoveredIndex == (int)i + 2) {
            Gdiplus::SolidBrush b(active ? COLOR_ITEM_ACTIVE : COLOR_ITEM_HOVER);
            g.FillRectangle(&b, r);
        }

        DrawSmoothAvatar(g, r.X + 10, r.Y + 6, dmUsers[i].username, dmUsers[i].online);

        Gdiplus::FontFamily nameFontFamily(L"Segoe UI");
        Gdiplus::Font nameFont(&nameFontFamily, 11, active ? Gdiplus::FontStyleBold : Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
        Gdiplus::SolidBrush nameBrush(active ? COLOR_TEXT_BRIGHT : COLOR_TEXT_NORMAL);
        Gdiplus::RectF textRect(r.X + 50, r.Y, r.Width - 50, r.Height);
        Gdiplus::StringFormat format;
        format.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        std::wstring wname(dmUsers[i].username.begin(), dmUsers[i].username.end());
        g.DrawString(wname.c_str(), -1, &nameFont, textRect, &format, &nameBrush);

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
        // SidebarFriends.cpp в цикле обработки клика:
        if (y >= cy && y <= cy + ITEM_HEIGHT) {
            if (g_uiState.activeChatUser == u.username) return;

            g_uiState.activeChatUser = u.username;
            g_uiState.currentPage = AppPage::Messages;

            // Загружаем из кэша
            messages = chatHistories[u.username];

            g_historyOffset = 0;
            g_isLoadingHistory = false; 
            RequestChatHistory(u.username, 0); 
            
            ShowChatUI(true);

            // Скроллим вниз МГНОВЕННО
            ScrollMessagesToBottom(); 

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
