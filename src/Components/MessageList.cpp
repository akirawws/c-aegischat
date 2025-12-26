#include "MessageList.h"
#include "Utils/Styles.h"
#include "Utils/Utils.h"
#include "Utils/UIState.h"
#include <algorithm>
#include <gdiplus.h>
#include <string>
#include <map>

using namespace Gdiplus;

HWND hMessageList = NULL;
extern std::map<std::string, std::vector<Message>> chatHistories;
extern UIState g_uiState;
int scrollPos = 0;

// Константы цветов Discord
#define DC_COLOR_BG           Color(255, 49, 51, 56)
#define DC_COLOR_TEXT_MAIN    Color(255, 219, 222, 225)
#define DC_COLOR_TEXT_MUTED   Color(255, 148, 155, 164)
#define DC_COLOR_USERNAME     Color(255, 255, 255, 255)
#define DC_COLOR_AVATAR_BG    Color(255, 88, 101, 242)

void DrawDiscordMessage(Graphics& g, const Message& msg, int& y, int w, bool isSameSender) {
    int avatarMargin = 16;
    int avatarSize = 40;
    int contentLeft = avatarMargin + avatarSize + 16;

    // 1. Очистка ника от символа '#'
    std::string cleanSender = msg.sender;
    cleanSender.erase(std::remove(cleanSender.begin(), cleanSender.end(), '#'), cleanSender.end());

    std::wstring wSender = Utf8ToWide(cleanSender);
    std::wstring wText = Utf8ToWide(msg.text);
    std::wstring wTime = Utf8ToWide(msg.timeStr);

    FontFamily fontFamily(L"Segoe UI");
    
    if (!isSameSender) {
        y += 12; // Отступ между блоками сообщений разных людей
        
        // --- Рисуем аватар ---
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        SolidBrush avatarBrush(DC_COLOR_AVATAR_BG);
        g.FillEllipse(&avatarBrush, (REAL)avatarMargin, (REAL)y, (REAL)avatarSize, (REAL)avatarSize);

        // Буква в аватаре
        Font letterFont(&fontFamily, 16, FontStyleBold, UnitPixel);
        StringFormat sf;
        sf.SetAlignment(StringAlignmentCenter);
        sf.SetLineAlignment(StringAlignmentCenter);
        SolidBrush whiteBrush(Color::White);
        std::wstring letter = wSender.empty() ? L"?" : wSender.substr(0, 1);
        RectF avatarRect((REAL)avatarMargin, (REAL)y, (REAL)avatarSize, (REAL)avatarSize);
        g.DrawString(letter.c_str(), -1, &letterFont, avatarRect, &sf, &whiteBrush);

        // --- Рисуем Никнейм ---
        // Используем ClearType для максимальной четкости
        g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
        Font nameFont(&fontFamily, 16, FontStyleBold, UnitPixel);
        SolidBrush nameBrush(DC_COLOR_USERNAME);
        g.DrawString(wSender.c_str(), -1, &nameFont, PointF((REAL)contentLeft, (REAL)y - 2), &nameBrush);

        // --- Рисуем Время ---
        RectF nameBounds;
        g.MeasureString(wSender.c_str(), -1, &nameFont, PointF(0, 0), &nameBounds);
        
        Font timeFont(&fontFamily, 12, FontStyleRegular, UnitPixel);
        SolidBrush timeBrush(DC_COLOR_TEXT_MUTED);

        g.DrawString(wTime.c_str(), -1, &timeFont, PointF((REAL)contentLeft + nameBounds.Width + 8, (REAL)y + 2), &timeBrush);

        y += 22; 
    }
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    Font msgFont(&fontFamily, 15, FontStyleRegular, UnitPixel);
    SolidBrush msgBrush(DC_COLOR_TEXT_MAIN);
    RectF textLayout((REAL)contentLeft, (REAL)y, (REAL)w - contentLeft - 40, 10000.0f);
    RectF boundingBox;
    g.DrawString(wText.c_str(), -1, &msgFont, textLayout, NULL, &msgBrush);
    g.MeasureString(wText.c_str(), -1, &msgFont, textLayout, &boundingBox);
    y += (int)boundingBox.Height + (isSameSender ? 4 : 10);
}

LRESULT CALLBACK MessageListWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_MOUSEWHEEL: {
            int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
            scrollPos -= zDelta / 2;
            if (scrollPos < 0) scrollPos = 0;
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rect;
            GetClientRect(hwnd, &rect);
            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBm = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
            HGDIOBJ oldBm = SelectObject(memDC, memBm);
            Graphics g(memDC);
            g.SetSmoothingMode(SmoothingModeAntiAlias);
            g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
            g.Clear(DC_COLOR_BG);
            auto& currentMsgs = chatHistories[g_uiState.activeChatUser];
            int drawY = 20 - scrollPos;

            for (size_t i = 0; i < currentMsgs.size(); ++i) {
                bool isSame = (i > 0 && currentMsgs[i-1].sender == currentMsgs[i].sender);
                DrawDiscordMessage(g, currentMsgs[i], drawY, rect.right, isSame);
            }
            BitBlt(hdc, 0, 0, rect.right, rect.bottom, memDC, 0, 0, SRCCOPY);
            
            SelectObject(memDC, oldBm);
            DeleteObject(memBm);
            DeleteDC(memDC);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_ERASEBKGND: 
            return 1;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}