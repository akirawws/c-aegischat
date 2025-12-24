#include "FriendsPage.h"
#include "Utils/Styles.h"
#include "Utils/Network.h"
#include "Utils/FriendsUtils.h"
#include <string>
#include <vector>

FriendsFilter currentFilter = FriendsFilter::Online;

// Структура для отображения заявок
struct PendingRequest {
    std::string username;
};

std::vector<PendingRequest> pendingRequests;




void DrawFriendsPage(HDC hdc, HWND hwnd, int width, int height) {
    int sidebarWidth = 72;
    int contentX = sidebarWidth;

    // --- Фон ---
    RECT bgRect = { contentX, 0, width, height };
    HBRUSH hBgBrush = CreateSolidBrush(COLOR_BG_DARK);
    FillRect(hdc, &bgRect, hBgBrush);
    DeleteObject(hBgBrush);

    // --- Header ---
    RECT headerRect = { contentX, 0, width, 48 };
    HBRUSH hHeaderBrush = CreateSolidBrush(RGB(49, 51, 56));
    FillRect(hdc, &headerRect, hHeaderBrush);

    HPEN hPen = CreatePen(PS_SOLID, 1, RGB(30, 31, 34));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    MoveToEx(hdc, contentX, 48, NULL);
    LineTo(hdc, width, 48);

    // --- Заголовок и фильтры ---
    SetBkMode(hdc, TRANSPARENT);
    HFONT hFontBold = CreateAppFont(14, FONT_WEIGHT_BOLD);
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFontBold);

    RECT textFriends = { contentX + 15, 0, contentX + 100, 48 };
    SetTextColor(hdc, RGB(255, 255, 255));
    DrawTextW(hdc, L"Друзья", -1, &textFriends, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

    MoveToEx(hdc, contentX + 85, 15, NULL);
    LineTo(hdc, contentX + 85, 33);

    const wchar_t* filters[] = { L"В сети", L"Все", L"Ожидание" };
    int startX = contentX + 100;
    for (int i = 0; i < 3; i++) {
        bool isActive = ((int)currentFilter == i);
        SetTextColor(hdc, isActive ? RGB(255, 255, 255) : RGB(181, 186, 193));
        RECT r = { startX, 10, startX + 80, 38 };
        if (isActive) {
            HBRUSH hActive = CreateSolidBrush(RGB(67, 71, 78));
            FillRect(hdc, &r, hActive);
            DeleteObject(hActive);
        }
        DrawTextW(hdc, filters[i], -1, &r, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
        startX += 90;
    }

    // --- Кнопка добавить в друзья ---
    RECT btnAdd = { width - 170, 10, width - 20, 38 };
    HBRUSH hBtnBrush = CreateSolidBrush(RGB(36, 128, 70));
    RoundRect(hdc, btnAdd.left, btnAdd.top, btnAdd.right, btnAdd.bottom, 5, 5);
    SetTextColor(hdc, RGB(255, 255, 255));
    DrawTextW(hdc, L"Добавить в друзья", -1, &btnAdd, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    DeleteObject(hBtnBrush);

    // --- Список ожиданий (если фильтр = Ожидание) ---
    if (currentFilter == FriendsFilter::Pending) {
        int y = 60;
        int itemHeight = 40;
        for (auto& req : pendingRequests) {
            RECT r = { contentX + 10, y, width - 10, y + itemHeight };
            HBRUSH hItemBrush = CreateSolidBrush(RGB(67, 71, 78));
            FillRect(hdc, &r, hItemBrush);
            DeleteObject(hItemBrush);

            SetTextColor(hdc, RGB(255, 255, 255));
            DrawTextA(hdc, req.username.c_str(), -1, &r, DT_SINGLELINE | DT_VCENTER | DT_LEFT);

            // Кнопки принять и отклонить
            RECT btnAccept = { r.right - 80, r.top + 5, r.right - 45, r.bottom - 5 };
            RECT btnReject = { r.right - 40, r.top + 5, r.right - 5, r.bottom - 5 };
            HBRUSH hGreen = CreateSolidBrush(RGB(36, 128, 70));
            FillRect(hdc, &btnAccept, hGreen);
            DeleteObject(hGreen);
            HBRUSH hRed = CreateSolidBrush(RGB(183, 28, 28));
            FillRect(hdc, &btnReject, hRed);
            DeleteObject(hRed);

            y += itemHeight + 5;
        }
    }

    // --- Заглушка если нет друзей ---
    if (pendingRequests.empty() && currentFilter == FriendsFilter::Pending) {
        RECT emptyRect = { contentX, 48, width, height };
        SetTextColor(hdc, RGB(148, 155, 164));
        DrawTextW(hdc, L"Заявок пока нет.", -1, &emptyRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    // Очистка
    SelectObject(hdc, hOldFont);
    SelectObject(hdc, hOldPen);
    DeleteObject(hFontBold);
    DeleteObject(hHeaderBrush);
    DeleteObject(hPen);
}

void HandleFriendsClick(HWND hwnd, int x, int y, HINSTANCE hInstance) {
    int sidebarWidth = 72;

    // --- Переключение фильтров ---
    int startX = sidebarWidth + 100;
    for (int i = 0; i < 3; i++) {
        RECT r = { startX, 10, startX + 80, 38 };
        if (x >= r.left && x <= r.right && y >= r.top && y <= r.bottom) {
            currentFilter = (FriendsFilter)i;
            InvalidateRect(hwnd, NULL, TRUE);
            return;
        }
        startX += 90;
    }

    // --- Кнопка "Добавить в друзья" ---
    RECT rect;
    GetClientRect(hwnd, &rect);
    RECT btnAdd = { rect.right - 170, 10, rect.right - 20, 38 };
    if (x >= btnAdd.left && x <= btnAdd.right && y >= btnAdd.top && y <= btnAdd.bottom) {
            AddFriendModalResult res = ShowAddFriendModal(hInstance, hwnd);
            if (res.confirmed && !res.username.empty()) {
                SendFriendRequest(res.username);
            }
    }

    // --- Кнопки принять/отклонить заявки ---
    if (currentFilter == FriendsFilter::Pending) {
        int yPos = 60;
        int itemHeight = 40;
        for (size_t i = 0; i < pendingRequests.size(); i++) {
            RECT btnAccept = { rect.right - 80, yPos + 5, rect.right - 45, yPos + itemHeight - 5 };
            RECT btnReject = { rect.right - 40, yPos + 5, rect.right - 5, yPos + itemHeight - 5 };

            if (x >= btnAccept.left && x <= btnAccept.right && y >= btnAccept.top && y <= btnAccept.bottom) {
                AcceptFriendRequest(pendingRequests[i].username);
                pendingRequests.erase(pendingRequests.begin() + i);
                InvalidateRect(hwnd, NULL, TRUE);
                return;
            }

            if (x >= btnReject.left && x <= btnReject.right && y >= btnReject.top && y <= btnReject.bottom) {
                RejectFriendRequest(pendingRequests[i].username);
                pendingRequests.erase(pendingRequests.begin() + i);
                InvalidateRect(hwnd, NULL, TRUE);
                return;
            }

            yPos += itemHeight + 5;
        }
    }
}
