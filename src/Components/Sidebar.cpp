#include "Sidebar.h"
#include "Utils/Styles.h"

HWND hSidebar = NULL;

HWND CreateSidebar(HWND hParent, int x, int y, int width, int height) {
    hSidebar = CreateWindowA("SidebarWindow", "",
        WS_VISIBLE | WS_CHILD,
        x, y, width, height, hParent, NULL, NULL, NULL);
    return hSidebar;
}

void OnPaintSidebar(HDC hdc, int width, int height) {
    RECT rect;
    GetClientRect(hSidebar, &rect);
    
    HBRUSH brush = CreateSolidBrush(COLOR_BG_SIDEBAR);
    FillRect(hdc, &rect, brush);
    DeleteObject(brush);
    
    SetTextColor(hdc, COLOR_TEXT_WHITE);
    SetBkMode(hdc, TRANSPARENT);
    HFONT hFont = CreateAppFont(FONT_SIZE_MEDIUM, FONT_WEIGHT_BOLD);
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    
    RECT textRect = { 24, 24, width - 24, 56 };
    DrawTextA(hdc, "Chats", -1, &textRect, DT_LEFT | DT_VCENTER);
    
    HPEN linePen = CreatePen(PS_SOLID, 1, COLOR_INPUT_BORDER);
    HPEN oldPen = (HPEN)SelectObject(hdc, linePen);
    MoveToEx(hdc, 16, 68, NULL);
    LineTo(hdc, width - 16, 68);
    SelectObject(hdc, oldPen);
    DeleteObject(linePen);
    
    RECT chatRect = { 12, 80, width - 12, 120 };
    brush = CreateSolidBrush(COLOR_BG_BLUE);
    FillRect(hdc, &chatRect, brush);
    DeleteObject(brush);
    
    HPEN borderPen = CreatePen(PS_SOLID, 1, COLOR_ACCENT_BLUE);
    SelectObject(hdc, borderPen);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    RoundRect(hdc, chatRect.left, chatRect.top, chatRect.right, chatRect.bottom, 8, 8);
    SelectObject(hdc, oldPen);
    DeleteObject(borderPen);
    
    SetTextColor(hdc, COLOR_TEXT_WHITE);
    SelectObject(hdc, hOldFont);
    hFont = CreateAppFont(FONT_SIZE_NORMAL, FONT_WEIGHT_SEMIBOLD);
    SelectObject(hdc, hFont);
    
    textRect = { 24, 88, width - 24, 112 };
    DrawTextA(hdc, "General Chat", -1, &textRect, DT_LEFT | DT_VCENTER);
    
    DeleteObject(hFont);
}

LRESULT CALLBACK SidebarWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rect;
        GetClientRect(hwnd, &rect);
        OnPaintSidebar(hdc, rect.right, rect.bottom);
        EndPaint(hwnd, &ps);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

