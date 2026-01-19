#include "MessageList.h"
#include "Utils/Styles.h"
#include "Utils/Utils.h"
#include "Utils/UIState.h"
#include <algorithm>
#include <gdiplus.h>
#include <string>
#include <map>
#include <filesystem> // Для проверки наличия файла аватара
#include "MainPage.h" 

namespace fs = std::filesystem;
using namespace Gdiplus;

// Глобальные и внешние переменные
HWND hMessageList = NULL;
extern HWND hMainWnd; 
extern std::map<std::string, ChatCache> chatHistories;
extern UIState g_uiState;
int scrollPos = 0;
float g_currentScroll = 0.0f; // Текущая позиция (плавная)
float g_targetScroll = 0.0f;  // Точка, к которой стремимся
const float SCROLL_SMOOTHNESS = 0.15f; // Скорость доводки (0.1 - медленно, 0.3 - быстро)


// Цвета в стиле Discord
#define DC_COLOR_BG           Color(255, 49, 51, 56)
#define DC_COLOR_TEXT_MAIN    Color(255, 219, 222, 225)
#define DC_COLOR_TEXT_MUTED   Color(255, 148, 155, 164)
#define DC_COLOR_USERNAME     Color(255, 255, 255, 255)
#define DC_COLOR_AVATAR_BG    Color(255, 88, 101, 242)

// Расчет общей высоты сообщений для скролла
int CalculateTotalMessageHeight(const std::vector<Message>& msgs) {
    if (msgs.empty()) return 0;

    HDC hdc = GetDC(hMessageList ? hMessageList : hMainWnd);
    if (!hdc) return 0;

    Graphics g(hdc);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

    int y = 20; 
    for (size_t i = 0; i < msgs.size(); ++i) {
        bool isSame = (i > 0 && msgs[i-1].sender == msgs[i].sender);
        int avatarSize = 40;
        int avatarMargin = 16;
        int contentLeft = avatarMargin + avatarSize + 16;

        if (!isSame) {
            y += 12 + 22; // Отступы и место под имя
        }

        FontFamily fontFamily(L"Segoe UI");
        Font msgFont(&fontFamily, 15, FontStyleRegular, UnitPixel);
        
        // Замеряем высоту текста сообщения с учетом переноса
        RectF textLayout((REAL)contentLeft, 0, (REAL)(10000), 10000.0f); 
        RectF boundingBox;
        std::wstring wText = Utf8ToWide(msgs[i].text);
        g.MeasureString(wText.c_str(), -1, &msgFont, textLayout, &boundingBox);
        
        y += (int)boundingBox.Height + (isSame ? 4 : 10);
    }

    ReleaseDC(hMessageList ? hMessageList : hMainWnd, hdc);
    return y;
}
// В MessageList.cpp
// MessageList.cpp
void ScrollMessagesToBottom() {
    if (!hMessageList || !IsWindow(hMessageList)) return;

    auto& msgs = chatHistories[g_uiState.activeChatUser].messages;
    int totalH = CalculateTotalMessageHeight(msgs);
    
    RECT rc;
    GetClientRect(hMessageList, &rc);
    int viewH = rc.bottom - rc.top;

    scrollPos = (totalH > viewH) ? (totalH - viewH) : 0;

    InvalidateRect(hMessageList, NULL, FALSE);
    UpdateWindow(hMessageList); 
}

// ОСНОВНАЯ ФУНКЦИЯ ОТРИСОВКИ СООБЩЕНИЯ
void DrawDiscordMessage(Graphics& g, const Message& msg, int& y, int w, bool isSameSender) {
    int avatarMargin = 16;
    int avatarSize = 40;
    int contentLeft = avatarMargin + avatarSize + 16;

    std::wstring wSender = Utf8ToWide(msg.sender);
    std::wstring wText = Utf8ToWide(msg.text);
    std::wstring wTime = Utf8ToWide(msg.timeStr);

    FontFamily fontFamily(L"Segoe UI");
    
    if (!isSameSender) {
        y += 12;
        
        RectF avatarRect((REAL)avatarMargin, (REAL)y, (REAL)avatarSize, (REAL)avatarSize);
        bool avatarDrawn = false;

        // 1. Попытка загрузить реальный аватар
        std::string avatarPath = GetUserAvatarPath(msg.sender);
        if (!avatarPath.empty() && fs::exists(avatarPath)) {
            std::wstring avatarPathW = Utf8ToWide(avatarPath);
            Image* img = Image::FromFile(avatarPathW.c_str());
            if (img && img->GetLastStatus() == Ok) {
                // Создаем круглую маску
                GraphicsPath path;
                path.AddEllipse(avatarRect);
                Region clipRegion(&path);
                
                GraphicsState state = g.Save();
                g.SetClip(&clipRegion, CombineModeReplace);
                g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
                g.SetSmoothingMode(SmoothingModeAntiAlias);
                
                g.DrawImage(img, avatarRect);
                
                g.Restore(state);
                avatarDrawn = true;
            }
            delete img;
        }

        // 2. Если аватара нет — рисуем заглушку с буквой
        if (!avatarDrawn) {
            g.SetSmoothingMode(SmoothingModeAntiAlias);
            SolidBrush avatarBrush(DC_COLOR_AVATAR_BG);
            g.FillEllipse(&avatarBrush, avatarRect);

            Font letterFont(&fontFamily, 16, FontStyleBold, UnitPixel);
            StringFormat sf;
            sf.SetAlignment(StringAlignmentCenter);
            sf.SetLineAlignment(StringAlignmentCenter);
            SolidBrush whiteBrush(Color::White);
            
            std::wstring letter = wSender.empty() ? L"?" : wSender.substr(0, 1);
            g.DrawString(letter.c_str(), -1, &letterFont, avatarRect, &sf, &whiteBrush);
        }

        // 3. Имя пользователя
        g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
        Font nameFont(&fontFamily, 16, FontStyleBold, UnitPixel);
        SolidBrush nameBrush(DC_COLOR_USERNAME);
        g.DrawString(wSender.c_str(), -1, &nameFont, PointF((REAL)contentLeft, (REAL)y - 2), &nameBrush);

        // 4. Время сообщения
        RectF nameBounds;
        g.MeasureString(wSender.c_str(), -1, &nameFont, PointF(0, 0), &nameBounds);
        
        Font timeFont(&fontFamily, 12, FontStyleRegular, UnitPixel);
        SolidBrush timeBrush(DC_COLOR_TEXT_MUTED);
        g.DrawString(wTime.c_str(), -1, &timeFont, PointF((REAL)contentLeft + nameBounds.Width + 8, (REAL)y + 2), &timeBrush);

        y += 22; 
    }

    // 5. Текст сообщения
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    Font msgFont(&fontFamily, 15, FontStyleRegular, UnitPixel);
    SolidBrush msgBrush(DC_COLOR_TEXT_MAIN);
    
    // Ограничиваем ширину текста, чтобы он не уходил за край
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
            
            auto& msgs = chatHistories[g_uiState.activeChatUser].messages;
            int totalH = CalculateTotalMessageHeight(msgs);
            RECT rc; GetClientRect(hwnd, &rc);
            int viewH = rc.bottom - rc.top;
            int maxScroll = std::max(0, totalH - viewH);

            // МГНОВЕННОЕ изменение
            scrollPos -= (zDelta / 120) * 40; // 40px на одно колесо (как в Discord)

            if (scrollPos < 0) scrollPos = 0;
            if (scrollPos > maxScroll) scrollPos = maxScroll;

            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
                case WM_TIMER: {
            if (wParam == 1) {
                // Вычисляем разницу между текущим и целевым положением
                float diff = g_targetScroll - g_currentScroll;

                if (std::abs(diff) > 0.5f) {
                    // Линейная интерполяция (Lerp)
                    g_currentScroll += diff * SCROLL_SMOOTHNESS;
                    
                    // Обновляем старую переменную для совместимости с отрисовкой
                    scrollPos = (int)g_currentScroll;
                    
                    InvalidateRect(hwnd, NULL, FALSE);
                } else {
                    // Если почти доехали — останавливаемся точно в цели
                    g_currentScroll = g_targetScroll;
                    scrollPos = (int)g_currentScroll;
                    KillTimer(hwnd, 1);
                    InvalidateRect(hwnd, NULL, FALSE);
                }
            }
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rect;
            GetClientRect(hwnd, &rect);

            // Двойная буферизация для отсутствия мерцания
            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBm = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
            HGDIOBJ oldBm = SelectObject(memDC, memBm);
            
            Graphics g(memDC);
            g.Clear(DC_COLOR_BG);

            auto& cache = chatHistories[g_uiState.activeChatUser];
            auto& currentMsgs = cache.messages;
            int drawY = 20 - scrollPos;

            for (size_t i = 0; i < currentMsgs.size(); ++i) {
                bool isSame = (i > 0 && currentMsgs[i-1].sender == currentMsgs[i].sender);
                DrawDiscordMessage(g, currentMsgs[i], drawY, rect.right, isSame);
            }

            // Копируем из буфера на экран
            BitBlt(hdc, 0, 0, rect.right, rect.bottom, memDC, 0, 0, SRCCOPY);
            
            SelectObject(memDC, oldBm);
            DeleteObject(memBm);
            DeleteDC(memDC);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_ERASEBKGND: 
            return 1; // Предотвращаем мерцание
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}