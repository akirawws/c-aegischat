#include "FriendsPage.h"
#include "Utils/Styles.h"
#include "Utils/Network.h"
#include "Utils/FriendsUtils.h"
#include "Components/SidebarFriends.h"
#include <vector>
#include <objidl.h>
#include <mutex>

const int SIDEBAR_ICONS_W = 72;
const int SIDEBAR_DM_W    = 240;
const int CONTENT_START_X = SIDEBAR_ICONS_W + SIDEBAR_DM_W; 

FriendsFilter currentFilter = FriendsFilter::Online;
std::vector<PendingRequest> pendingRequests;
std::mutex pendingMutex;

#define COLOR_BG            RGB(32, 34, 37)
#define COLOR_HEADER        RGB(49, 51, 56)
#define COLOR_CARD          RGB(54, 57, 63)
#define COLOR_CARD_HOVER    RGB(64, 68, 75)
#define COLOR_TEXT_MAIN     RGB(255, 255, 255)
#define COLOR_TEXT_MUTED    RGB(180, 185, 193)

#define COLOR_GREEN         RGB(67, 181, 129)
#define COLOR_RED           RGB(240, 71, 71)
#define COLOR_BUTTON        RGB(88, 101, 242)


void DrawCenteredText(HDC hdc, const wchar_t* text, RECT r, COLORREF color, HFONT font) {
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    SelectObject(hdc, font);
    DrawTextW(hdc, text, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}


void DrawFriendsPage(HDC hdc, HWND hwnd, int width, int height) {
    RECT bg = { CONTENT_START_X, 0, width, height };
    HBRUSH hBrBg = CreateSolidBrush(COLOR_BG);
    FillRect(hdc, &bg, hBrBg);
    DeleteObject(hBrBg);

    RECT header = { CONTENT_START_X, 0, width, 56 };
    HBRUSH hBrHead = CreateSolidBrush(COLOR_HEADER);
    FillRect(hdc, &header, hBrHead);
    DeleteObject(hBrHead);

    HFONT fontTitle = CreateAppFont(16, FW_BOLD);
    HFONT fontNormal = CreateAppFont(13, FW_NORMAL);

    RECT titleRect = { CONTENT_START_X + 16, 0, CONTENT_START_X + 150, 56 };
    DrawCenteredText(hdc, L"Друзья", titleRect, COLOR_TEXT_MAIN, fontTitle);

    const wchar_t* filters[] = { L"В сети", L"Все", L"Ожидание" };
    int fx = CONTENT_START_X + 160;

    for (int i = 0; i < 3; i++) {
        RECT fr = { fx, 12, fx + 90, 44 };
        if ((int)currentFilter == i)
            DrawRoundedRect(hdc, fr, COLOR_CARD, 10);
        
        DrawCenteredText(
            hdc, filters[i], fr,
            (currentFilter == (FriendsFilter)i) ? COLOR_TEXT_MAIN : COLOR_TEXT_MUTED,
            fontNormal
        );
        fx += 100;
    }

    RECT btnAdd = { width - 200, 12, width - 20, 44 };
    DrawRoundedRect(hdc, btnAdd, COLOR_BUTTON, 10);
    DrawCenteredText(hdc, L"+ Добавить друга", btnAdd, COLOR_TEXT_MAIN, fontNormal);

if (currentFilter == FriendsFilter::Pending) {
        std::lock_guard<std::mutex> lock(pendingMutex);

        int yPos = 72; 
        for (const auto& req : pendingRequests) {
            RECT card = { CONTENT_START_X + 20, yPos, width - 20, yPos + 56 };
            DrawRoundedRect(hdc, card, COLOR_CARD, 8);

            RECT nameRect = { card.left + 15, card.top, card.right - 130, card.bottom };
            SetTextColor(hdc, COLOR_TEXT_MAIN);
            SelectObject(hdc, fontNormal);
            DrawTextA(hdc, req.username.c_str(), -1, &nameRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            RECT btnAccept = { card.right - 110, card.top + 12, card.right - 65, card.bottom - 12 };
            RECT btnReject = { card.right - 55, card.top + 12, card.right - 10, card.bottom - 12 };

            DrawRoundedRect(hdc, btnAccept, COLOR_GREEN, 6);
            DrawRoundedRect(hdc, btnReject, COLOR_RED, 6);

            DrawCenteredText(hdc, L"✔", btnAccept, COLOR_TEXT_MAIN, fontNormal);
            DrawCenteredText(hdc, L"✖", btnReject, COLOR_TEXT_MAIN, fontNormal);

            yPos += 66; 
        }

        if (pendingRequests.empty()) {
            RECT emptyRect = { CONTENT_START_X, 100, width, height };
            DrawCenteredText(hdc, L"Список ожидающих заявок пуст", emptyRect, COLOR_TEXT_MUTED, fontNormal);
        }
    }

    DeleteObject(fontTitle);
    DeleteObject(fontNormal);
}


void HandleFriendsClick(HWND hwnd, int x, int y, HINSTANCE hInstance) {
    int fx = CONTENT_START_X + 160; 

    // 1. Проверка кнопок фильтров (В сети, Все, Ожидание)
    for (int i = 0; i < 3; i++) {
        RECT r = { fx, 12, fx + 90, 44 };
        if (PtInRect(&r, { x, y })) {
            currentFilter = (FriendsFilter)i;
            InvalidateRect(hwnd, NULL, FALSE); 
            return;
        }
        fx += 100;
    }

    // 2. Проверка кнопки "Добавить друга"
    RECT rect;
    GetClientRect(hwnd, &rect);
    RECT btnAdd = { rect.right - 200, 12, rect.right - 20, 44 };

    if (PtInRect(&btnAdd, { x, y })) {
        auto res = ShowAddFriendModal(hInstance, hwnd);
        if (res.confirmed) {
            SendFriendRequest(res.username);
        }
        return;
    }

    // 3. Проверка кнопок в списке заявок (Accept/Reject)
    if (currentFilter == FriendsFilter::Pending) {
        std::lock_guard<std::mutex> lock(pendingMutex);
        int yPos = 72;
        for (size_t i = 0; i < pendingRequests.size(); i++) {
            RECT accept = { rect.right - 120, yPos + 12, rect.right - 70, yPos + 44 };
            RECT reject = { rect.right - 60, yPos + 12, rect.right - 12, yPos + 44 };

            if (PtInRect(&accept, { x, y })) {
                std::string acceptedName = pendingRequests[i].username;
                AcceptFriendRequest(acceptedName); 
                AddUserToDMList(hwnd, acceptedName, true); 

                pendingRequests.erase(pendingRequests.begin() + i);
                InvalidateRect(hwnd, NULL, FALSE);
                return;
            }
            if (PtInRect(&reject, { x, y })) {
                RejectFriendRequest(pendingRequests[i].username);
                pendingRequests.erase(pendingRequests.begin() + i);
                InvalidateRect(hwnd, NULL, FALSE);
                return;
            }
            yPos += 68;
        }
    }
}