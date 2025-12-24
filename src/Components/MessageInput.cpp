#include "MessageInput.h"
#include "Utils/Styles.h"
#include "Utils/Network.h"
#include "Utils/Utils.h"
#include <vector>
#include <algorithm>

HWND hInputEdit = NULL;
int inputEditHeight = 60;

HWND CreateMessageInput(HWND hParent, int x, int y, int width, int height) {
    hInputEdit = CreateWindowA("EDIT", "",
        WS_VISIBLE | WS_CHILD | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL,
        x, y, width, INPUT_MIN_HEIGHT, hParent, NULL, NULL, NULL);
    
    HFONT hInputFont = CreateInputFont(15, FONT_WEIGHT_NORMAL);
    SendMessage(hInputEdit, WM_SETFONT, (WPARAM)hInputFont, TRUE);
    SendMessage(hInputEdit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(16, 16));
    
    return hInputEdit;
}

void UpdateInputHeight(HWND hwnd, HWND hInputEdit, HWND hMessageList) {
    RECT rect;
    GetClientRect(hInputEdit, &rect);
    HDC hdc = GetDC(hInputEdit);
    HFONT hFont = CreateInputFont(15, FONT_WEIGHT_NORMAL);
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    
    int len = GetWindowTextLengthW(hInputEdit);
    int newHeight = INPUT_MIN_HEIGHT;

    if (len > 0) {
        std::vector<wchar_t> buffer(len + 1);
        GetWindowTextW(hInputEdit, buffer.data(), len + 1);
        RECT calcRect = { 0, 0, rect.right - 32, 0 };
        DrawTextW(hdc, buffer.data(), -1, &calcRect, DT_CALCRECT | DT_WORDBREAK | DT_EDITCONTROL);
        
        newHeight = (std::max)((int)INPUT_MIN_HEIGHT, (std::min)((int)INPUT_MAX_HEIGHT, (int)calcRect.bottom + 32));
    }
    
    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
    ReleaseDC(hInputEdit, hdc);
    
    if (newHeight != inputEditHeight) {
        inputEditHeight = newHeight;
        RECT mainRect;
        GetClientRect(hwnd, &mainRect);
        MoveWindow(hInputEdit, 260, mainRect.bottom - inputEditHeight - 10, 
                  mainRect.right - 270, inputEditHeight, TRUE);
        MoveWindow(hMessageList, 250, 0, mainRect.right - 250, 
                  mainRect.bottom - inputEditHeight - 20, TRUE);
        
        InvalidateRect(hwnd, NULL, TRUE);
    }
}

void DrawInputField(HDC hdc, HWND hwnd, HWND hInputEdit) {
    if (!hInputEdit) return;
    RECT inputRect;
    GetWindowRect(hInputEdit, &inputRect);
    ScreenToClient(hwnd, (LPPOINT)&inputRect);
    ScreenToClient(hwnd, ((LPPOINT)&inputRect) + 1);
    
    inputRect.left -= 2; inputRect.top -= 2;
    inputRect.right += 2; inputRect.bottom += 2;
    
    bool hasFocus = (GetFocus() == hInputEdit);
    HBRUSH inputBrush = CreateSolidBrush(COLOR_INPUT_BG);
    HPEN inputPen = CreatePen(PS_SOLID, hasFocus ? 2 : 1, hasFocus ? COLOR_ACCENT_BLUE : COLOR_INPUT_BORDER);
    
    HPEN oldPen = (HPEN)SelectObject(hdc, inputPen);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, inputBrush);
    RoundRect(hdc, inputRect.left, inputRect.top, inputRect.right, inputRect.bottom, 14, 14);
    
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(inputBrush);
    DeleteObject(inputPen);
}