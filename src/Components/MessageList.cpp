#include "MessageList.h"
#include "Utils/Styles.h"
#include <algorithm>

HWND hMessageList = NULL;
std::vector<Message> messages;
int scrollPos = 0;

HWND CreateMessageList(HWND hParent, int x, int y, int width, int height) {
    hMessageList = CreateWindowA("MessageListWindow", "",
        WS_VISIBLE | WS_CHILD | WS_VSCROLL,
        x, y, width, height, hParent, NULL, NULL, NULL);
    return hMessageList;
}

int GetMessageHeight(const Message& msg, int width) {
    HDC hdc = GetDC(hMessageList);
    int msgWidth = (int)(width * 0.70);
    RECT textRect = { 0, 0, msgWidth - 32, 0 };
    
    HFONT hFont = CreateAppFont(FONT_SIZE_NORMAL);
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    
    DrawTextA(hdc, msg.text.c_str(), -1, &textRect, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_CALCRECT);
    
    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
    ReleaseDC(hMessageList, hdc);

    return std::max((int)70, (int)(textRect.bottom + 50));
}

void DrawMessage(HDC hdc, const Message& msg, int y, int width) {
    int wLen = MultiByteToWideChar(CP_UTF8, 0, msg.text.c_str(), -1, NULL, 0);
    std::wstring wText(wLen, 0);
    MultiByteToWideChar(CP_UTF8, 0, msg.text.c_str(), -1, &wText[0], wLen);

    int sLen = MultiByteToWideChar(CP_UTF8, 0, msg.sender.c_str(), -1, NULL, 0);
    std::wstring wSender(sLen, 0);
    MultiByteToWideChar(CP_UTF8, 0, msg.sender.c_str(), -1, &wSender[0], sLen);

    int tLen = MultiByteToWideChar(CP_UTF8, 0, msg.timeStr.c_str(), -1, NULL, 0);
    std::wstring wTime(tLen, 0);
    MultiByteToWideChar(CP_UTF8, 0, msg.timeStr.c_str(), -1, &wTime[0], tLen);

    int maxBubbleWidth = (int)(width * 0.70);
    
    HFONT hFontMsg = CreateFontW(FONT_SIZE_NORMAL, 0, 0, 0, FONT_WEIGHT_NORMAL, FALSE, FALSE, FALSE, 
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, 
                                 CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    
    HDC hdcMeasure = CreateCompatibleDC(hdc);
    SelectObject(hdcMeasure, hFontMsg);
    
    RECT calcRect = { 0, 0, maxBubbleWidth - 36, 0 };
    DrawTextW(hdcMeasure, wText.c_str(), -1, &calcRect, DT_CALCRECT | DT_WORDBREAK);
    DeleteDC(hdcMeasure);

    int finalBubbleWidth = std::max((int)160, (int)calcRect.right + 36);
    int finalHeight = calcRect.bottom + 52; 
    
    int x = msg.isUser ? (width - finalBubbleWidth - 20) : 20;

    HBRUSH shadowBrush = CreateSolidBrush(RGB(0, 0, 0));
    RECT shadowRect = { x + 2, y + 2, x + finalBubbleWidth + 2, y + finalHeight + 2 };
    FillRect(hdc, &shadowRect, shadowBrush);
    DeleteObject(shadowBrush);
    
    HPEN borderPen = CreatePen(PS_SOLID, 1, msg.isUser ? COLOR_ACCENT_BLUE : COLOR_INPUT_BORDER);
    HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
    HBRUSH bubbleBrush = CreateSolidBrush(msg.isUser ? COLOR_MESSAGE_USER : COLOR_MESSAGE_OTHER);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, bubbleBrush);
    
    RoundRect(hdc, x, y, x + finalBubbleWidth, y + finalHeight, 18, 18);
    
    SelectObject(hdc, oldPen);
    DeleteObject(borderPen);

    SetBkMode(hdc, TRANSPARENT);
    
    HFONT hFontName = CreateFontW(FONT_SIZE_SMALL, 0, 0, 0, FONT_WEIGHT_SEMIBOLD, FALSE, FALSE, FALSE, 
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, 
                                  CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    SelectObject(hdc, hFontName);
    SetTextColor(hdc, msg.isUser ? COLOR_ACCENT_BLUE_LIGHT : COLOR_ACCENT_BLUE);
    
    TextOutW(hdc, x + 18, y + 12, wSender.c_str(), (int)wcslen(wSender.c_str()));

    SIZE nameSize;
    GetTextExtentPoint32W(hdc, wSender.c_str(), (int)wcslen(wSender.c_str()), &nameSize);

    HFONT hFontTime = CreateFontW(FONT_SIZE_SMALL - 1, 0, 0, 0, FONT_WEIGHT_NORMAL, FALSE, FALSE, FALSE, 
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, 
                                  CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    SelectObject(hdc, hFontTime);
    SetTextColor(hdc, COLOR_TEXT_GRAY);
    
    TextOutW(hdc, x + 18 + nameSize.cx + 10, y + 13, wTime.c_str(), (int)wcslen(wTime.c_str()));

    SelectObject(hdc, hFontMsg);
    SetTextColor(hdc, COLOR_TEXT_WHITE);
    RECT textRect = { x + 18, y + 32, x + finalBubbleWidth - 18, y + finalHeight - 12 };
    DrawTextW(hdc, wText.c_str(), -1, &textRect, DT_LEFT | DT_WORDBREAK | DT_TOP);

    SelectObject(hdc, oldBrush);
    DeleteObject(bubbleBrush);
    DeleteObject(hFontMsg);
    DeleteObject(hFontName);
    DeleteObject(hFontTime);
}

void OnPaintMessageList(HDC hdc, int width, int height) {
    RECT rect = { 0, 0, width, height };
    HBRUSH brush = CreateSolidBrush(COLOR_BG_DARK);
    FillRect(hdc, &rect, brush);
    DeleteObject(brush);
    
    int y = 20;
    for (const auto& msg : messages) {
        if (y + 60 > 0 && y < height) { 
            DrawMessage(hdc, msg, y, width);
        }
        y += GetMessageHeight(msg, width) + 10;
    }
}

LRESULT CALLBACK MessageListWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_MOUSEWHEEL: {
        int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
        scrollPos -= (zDelta / 2);
        if (scrollPos < 0) scrollPos = 0;
        
        RECT rect;
        GetClientRect(hwnd, &rect);
        int totalHeight = 20;
        for (const auto& m : messages) totalHeight += GetMessageHeight(m, rect.right) + 10;
        int maxScroll = std::max((int)0, (int)(totalHeight - rect.bottom + 50));
        if (scrollPos > maxScroll) scrollPos = maxScroll;

        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rect;
        GetClientRect(hwnd, &rect);

        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
        SelectObject(memDC, memBitmap);

        HBRUSH brush = CreateSolidBrush(COLOR_BG_DARK);
        FillRect(memDC, &rect, brush);
        DeleteObject(brush);

        int y = 20 - scrollPos; 
        for (const auto& msg : messages) {
            int h = GetMessageHeight(msg, rect.right);
            if (y + h > 0 && y < rect.bottom) {
                DrawMessage(memDC, msg, y, rect.right);
            }
            y += h + 10;
        }

        BitBlt(hdc, 0, 0, rect.right, rect.bottom, memDC, 0, 0, SRCCOPY);
        DeleteObject(memBitmap);
        DeleteDC(memDC);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

