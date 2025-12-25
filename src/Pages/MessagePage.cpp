#include "MessagePage.h"
#include "Utils/Styles.h"

#define COLOR_BG           RGB(54, 57, 63)
#define COLOR_MINE         RGB(88, 101, 242)
#define COLOR_OTHER        RGB(64, 68, 75)
#define COLOR_TEXT         RGB(220, 221, 222)
#define COLOR_TIME         RGB(150, 152, 157)


void DrawMessagePage(HDC hdc, int x, int y, int w, int h, const std::vector<Message>& messages) {
    // 1. Очистка фона
    RECT bg = { x, y, x + w, y + h };
    HBRUSH hBrBg = CreateSolidBrush(COLOR_BG);
    FillRect(hdc, &bg, hBrBg);
    DeleteObject(hBrBg);

    HFONT font = CreateAppFont(15, FW_NORMAL);
    HFONT fontTime = CreateAppFont(11, FW_NORMAL);
    
    int msgY = y + 20; 
    int maxBubbleWidth = (int)(w * 0.7); // 70% от ширины окна

    for (const auto& msg : messages) {
        // 2. Расчет размера текста
        RECT textCalc = { 0, 0, maxBubbleWidth - 30, 0 };
        SelectObject(hdc, font);
        DrawTextA(hdc, msg.text.c_str(), -1, &textCalc, DT_CALCRECT | DT_WORDBREAK);

        int bubbleW = textCalc.right + 30;
        int bubbleH = textCalc.bottom + 25; 

        // 3. ПОЗИЦИОНИРОВАНИЕ (Критический узел)
        RECT bubble;
        if (msg.isMine) {
            bubble.right = x + w - 20;
            bubble.left = bubble.right - bubbleW;
        } else {
            bubble.left = x + 20;
            bubble.right = bubble.left + bubbleW;
        }
        bubble.top = msgY;
        bubble.bottom = msgY + bubbleH;

        // 4. Отрисовка
        DrawRoundedRect(hdc, bubble, msg.isMine ? COLOR_MINE : COLOR_OTHER, 12);

        // Текст
        RECT textPos = { bubble.left + 15, bubble.top + 10, bubble.right - 15, bubble.bottom - 10 };
        SetTextColor(hdc, COLOR_TEXT);
        SetBkMode(hdc, TRANSPARENT);
        DrawTextA(hdc, msg.text.c_str(), -1, &textPos, DT_WORDBREAK);

        // Время (всегда внутри баббла в углу)
        SelectObject(hdc, fontTime);
        SetTextColor(hdc, COLOR_TIME);
        int timeX = bubble.right - 50;
        TextOutA(hdc, timeX, bubble.bottom - 18, msg.timeStr.c_str(), (int)msg.timeStr.length());

        msgY += bubbleH + 10; // Смещение для следующего сообщения
    }

    DeleteObject(font);
    DeleteObject(fontTime);
}

void DrawMessageInput(HDC hdc, int x, int y, int w, int h, const std::string& inputText) {
    RECT inputRect = { x, y, x + w, y + h };
    DrawRoundedRect(hdc, inputRect, RGB(64, 68, 75), 8);

    HFONT font = CreateAppFont(14, FW_NORMAL);
    HFONT oldFont = (HFONT)SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, COLOR_TEXT);

    RECT textRect = inputRect;
    textRect.left += 8;
    textRect.top += 4;

    DrawTextA(hdc, inputText.c_str(), -1, &textRect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    SelectObject(hdc, oldFont);
    DeleteObject(font);
}
