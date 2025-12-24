#include "Sidebar.h"
#include "Utils/Styles.h"
#include <string>
#include <vector>

HWND hSidebar = NULL;

// Перечисляем типы страниц для удобства
enum class PageType { Friends, Chat };

// Функция для отрисовки кругов (без изменений, добавлена для полноты)
void DrawDiscordCircle(HDC hdc, int x, int y, int radius, COLORREF color, const std::wstring& text, bool isEmoji) {
    HBRUSH brush = CreateSolidBrush(color);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
    HPEN pen = CreatePen(PS_NULL, 0, 0); 
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);

    Ellipse(hdc, x - radius, y - radius, x + radius, y + radius);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));
    
    HFONT hFont;
    if (isEmoji) {
        hFont = CreateFontW(26, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, 
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, 
                            DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Emoji");
    } else {
        hFont = CreateAppFont(14, FONT_WEIGHT_BOLD); 
    }
    
    HFONT oldFont = (HFONT)SelectObject(hdc, hFont);
    RECT textRect = { x - radius, y - radius, x + radius, y + radius };
    DrawTextW(hdc, text.c_str(), -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    SelectObject(hdc, oldFont);
    DeleteObject(hFont);
    SelectObject(hdc, oldBrush);
    DeleteObject(brush);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

void OnPaintSidebar(HDC hdc, int width, int height) {
    RECT rect = { 0, 0, width, height };
    HBRUSH bgBrush = CreateSolidBrush(COLOR_BG_SIDEBAR);
    FillRect(hdc, &rect, bgBrush);
    DeleteObject(bgBrush);

    int centerX = width / 2;
    
    // 1. Логотип/Щит (Открывает FriendsPage)
    DrawDiscordCircle(hdc, centerX, 35, 24, COLOR_BG_BLUE, L"🛡", true);

    // Разделитель
    HPEN pen = CreatePen(PS_SOLID, 2, RGB(50, 53, 59));
    SelectObject(hdc, pen);
    MoveToEx(hdc, centerX - 15, 70, NULL);
    LineTo(hdc, centerX + 15, 70);
    DeleteObject(pen);

    // 2. Список чатов (Открывают MessagePage)
    int currentY = 105;
    const wchar_t* chatNames[] = { L"gen", L"dev", L"news", L"off" };
    COLORREF colors[] = { COLOR_ACCENT_BLUE, RGB(88, 101, 242), RGB(67, 181, 129), RGB(114, 137, 218) };

    for (int i = 0; i < 4; i++) {
        DrawDiscordCircle(hdc, centerX, currentY, 24, colors[i], chatNames[i], false);
        currentY += 60; 
    }
}
HWND CreateSidebar(HWND hParent, int x, int y, int width, int height) {
    hSidebar = CreateWindowA("SidebarWindow", "",
        WS_VISIBLE | WS_CHILD,
        x, y, width, height, hParent, NULL, (HINSTANCE)GetWindowLongPtr(hParent, GWLP_HINSTANCE), NULL);
        
    return hSidebar;
}


LRESULT CALLBACK SidebarWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rect;
            GetClientRect(hwnd, &rect);
            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
            HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

            OnPaintSidebar(memDC, rect.right, rect.bottom);

            BitBlt(hdc, 0, 0, rect.right, rect.bottom, memDC, 0, 0, SRCCOPY);
            SelectObject(memDC, oldBitmap);
            DeleteObject(memBitmap);
            DeleteDC(memDC);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_LBUTTONDOWN: {
            int y = HIWORD(lParam);
            HWND hMain = GetParent(hwnd);

            // Клик по Щиту (FriendsPage)
            if (y >= 10 && y <= 60) {
                SendMessage(hMain, WM_USER + 2, 0, 0); // Сигнал: открыть Friends
            }
            // Клик по иконкам чатов
            else if (y >= 80 && y <= 350) {
                SendMessage(hMain, WM_USER + 1, 0, 0); // Сигнал: открыть Chat
            }
            return 0;
        }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}