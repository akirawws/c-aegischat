#include "MessageInput.h"
#include "Utils/Styles.h"
#include "Utils/Network.h"
#include "Utils/Utils.h"
#include <vector>
#include <algorithm>
#include <commctrl.h> 

#define COLOR_DISCORD_INPUT_BG    RGB(56, 58, 64)
#define COLOR_DISCORD_TEXT        RGB(220, 221, 222)
#define COLOR_DISCORD_PLACEHOLDER RGB(148, 155, 164)
#define COLOR_DISCORD_PLUS        RGB(181, 186, 193)

HWND hInputEdit = NULL;
int inputEditHeight = 44; 

HWND CreateMessageInput(HWND hParent, int x, int y, int width, int height) {
    hInputEdit = CreateWindowExA(0, "EDIT", "",
        WS_VISIBLE | WS_CHILD | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL,
        x + 50, y + 12, width - 65, height - 24, hParent, NULL, NULL, NULL);
    
    HFONT hInputFont = CreateAppFont(15, FW_NORMAL);
    SendMessage(hInputEdit, WM_SETFONT, (WPARAM)hInputFont, TRUE);

    SendMessageW(hInputEdit, EM_SETCUEBANNER, TRUE, (LPARAM)L"Написать сообщение...");
    
    SendMessage(hInputEdit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(0, 0));
    
    return hInputEdit;
}

void UpdateInputHeight(HWND hwnd, HWND hInputEdit, HWND hMessageList) {
    if (!hInputEdit) return;

    RECT rect;
    GetClientRect(hInputEdit, &rect);
    HDC hdc = GetDC(hInputEdit);
    HFONT hFont = CreateAppFont(15, FW_NORMAL);
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    
    int len = GetWindowTextLengthW(hInputEdit);
    int newHeight = 44;

    std::vector<wchar_t> buffer(len + 1);
    GetWindowTextW(hInputEdit, buffer.data(), len + 1);
    
    RECT calcRect = { 0, 0, rect.right, 0 };
    DrawTextW(hdc, buffer.data(), -1, &calcRect, DT_CALCRECT | DT_WORDBREAK | DT_EDITCONTROL);
 
    newHeight = (std::max)(44, (std::min)(200, (int)calcRect.bottom + 24));
    
    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
    ReleaseDC(hInputEdit, hdc);
    
    if (newHeight != inputEditHeight) {
        inputEditHeight = newHeight;
        RECT mainRect;
        GetClientRect(hwnd, &mainRect);

        int inputX = 312 + 16;
        int inputW = mainRect.right - inputX - 16;
        int inputY = mainRect.bottom - inputEditHeight - 20;

        MoveWindow(hInputEdit, inputX + 50, inputY + 12, inputW - 65, inputEditHeight - 24, TRUE);
        
        MoveWindow(hMessageList, 312, 0, mainRect.right - 312, inputY - 10, TRUE);
        
        InvalidateRect(hwnd, NULL, TRUE);
    }
}

void DrawInputField(HDC hdc, HWND hwnd, HWND hInputEdit) {
    if (!hInputEdit) return;

    RECT editRect;
    GetWindowRect(hInputEdit, &editRect);
    ScreenToClient(hwnd, (LPPOINT)&editRect);
    ScreenToClient(hwnd, ((LPPOINT)&editRect) + 1);


    RECT container = { editRect.left - 50, editRect.top - 12, editRect.right + 15, editRect.bottom + 12 };

    HBRUSH hBrInput = CreateSolidBrush(COLOR_DISCORD_INPUT_BG);
    HBRUSH oldBr = (HBRUSH)SelectObject(hdc, hBrInput);
    HPEN hPen = CreatePen(PS_NULL, 0, 0);
    HPEN oldPen = (HPEN)SelectObject(hdc, hPen);

    RoundRect(hdc, container.left, container.top, container.right, container.bottom, 10, 10);

    int circleSize = 24;
    int cx = container.left + 12;
    int cy = container.top + ( (container.bottom - container.top) - circleSize ) / 2;
    
    HBRUSH hBrPlus = CreateSolidBrush(COLOR_DISCORD_PLUS);
    SelectObject(hdc, hBrPlus);
    Ellipse(hdc, cx, cy, cx + circleSize, cy + circleSize);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, COLOR_DISCORD_INPUT_BG);
    HFONT hFontPlus = CreateAppFont(20, FW_BOLD);
    SelectObject(hdc, hFontPlus);
    RECT plusRect = { cx, cy - 1, cx + circleSize, cy + circleSize };
    DrawTextA(hdc, "+", -1, &plusRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
    DeleteObject(hBrInput);
    DeleteObject(hBrPlus);
    DeleteObject(hPen);
    DeleteObject(hFontPlus);
}