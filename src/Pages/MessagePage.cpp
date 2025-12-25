#include "MessagePage.h"
#include "Utils/Styles.h"

#define COLOR_BG           RGB(54, 57, 63)
#define COLOR_MINE         RGB(88, 101, 242)
#define COLOR_OTHER        RGB(64, 68, 75)
#define COLOR_TEXT         RGB(220, 221, 222)
#define COLOR_TIME         RGB(150, 152, 157)


void DrawMessagePage(HDC hdc, int x, int y, int w, int h, const std::vector<Message>& messages) {
    // Чистый фон без мерцания
    RECT bg = { x, y, x + w, y + h };
    HBRUSH hBrBg = CreateSolidBrush(COLOR_BG);
    FillRect(hdc, &bg, hBrBg);
    DeleteObject(hBrBg);

    HFONT font = CreateAppFont(15, FW_NORMAL);
    HFONT fontTime = CreateAppFont(11, FW_NORMAL);
    
    int msgY = y + 20; // Начальный отступ сверху
    int maxBubbleWidth = w * 0.7; // Сообщение занимает максимум 70% ширины

    for (const auto& msg : messages) {
        // Вычисляем размер текста
        RECT textCalc = { 0, 0, maxBubbleWidth - 30, 0 };
        SelectObject(hdc, font);
        DrawTextA(hdc, msg.text.c_str(), -1, &textCalc, DT_CALCRECT | DT_WORDBREAK);

        int bubbleW = textCalc.right + 30;
        int bubbleH = textCalc.bottom + 25; // Запас под время

        RECT bubble;
        if (msg.isMine) {
            bubble = { x + w - bubbleW - 20, msgY, x + w - 20, msgY + bubbleH };
        } else {
            bubble = { x + 20, msgY, x + 20 + bubbleW, msgY + bubbleH };
        }

        // Рисуем тень или свечение (опционально) и само облако
        DrawRoundedRect(hdc, bubble, msg.isMine ? COLOR_MINE : COLOR_OTHER, 12);

        // Текст сообщения
        RECT textPos = { bubble.left + 15, bubble.top + 10, bubble.right - 15, bubble.bottom - 10 };
        SetTextColor(hdc, COLOR_TEXT);
        SetBkMode(hdc, TRANSPARENT);
        DrawTextA(hdc, msg.text.c_str(), -1, &textPos, DT_WORDBREAK);

        // Время в углу
        SelectObject(hdc, fontTime);
        SetTextColor(hdc, COLOR_TIME);
        std::string time = msg.time; // Используем время из структуры
        TextOutA(hdc, bubble.right - 45, bubble.bottom - 18, time.c_str(), time.length());

        msgY += bubbleH + 10; // Расстояние между сообщениями
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
