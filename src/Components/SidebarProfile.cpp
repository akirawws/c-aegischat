#include "SidebarProfile.h"
#include "Utils/UIState.h" // Важно для доступа к g_uiState
#include "Utils/Utils.h"   // Если у вас там Utf8ToWide
#include <windows.h>

using namespace Gdiplus;

void DrawSidebarProfile(Graphics& g, int x, int windowHeight, int totalWidth, const std::string& fallbackName) {
    // Используем REAL для дробных координат, чтобы избежать дрожания пикселей
    REAL rectX = (REAL)x;
    REAL rectY = (REAL)(windowHeight - PROFILE_PANEL_HEIGHT);
    REAL rectW = (REAL)totalWidth;
    REAL rectH = (REAL)PROFILE_PANEL_HEIGHT;

    // --- УСТРАНЕНИЕ ПИКСЕЛЬНОСТИ ---
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit); // Четкий текст на темном фоне
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);

    // 1. Единый фон на оба сайдбара
    SolidBrush bgBrush(Color(255, 35, 36, 40)); 
    g.FillRectangle(&bgBrush, rectX, rectY, rectW, rectH);

    // 2. Аватар
    float avatarSize = 32.0f;
    float margin = (rectH - avatarSize) / 2.0f;
    // Рисуем аватар с небольшим отступом слева
    RectF avatarRect(rectX + 12.0f, rectY + margin, avatarSize, avatarSize);
    
    SolidBrush avatarBrush(Color(255, 88, 101, 242)); 
    g.FillEllipse(&avatarBrush, avatarRect);

    // 3. Текст (Имя и Статус)
    std::string nameToDraw = g_uiState.userDisplayName.empty() ? fallbackName : g_uiState.userDisplayName;

    FontFamily fontFamily(L"Segoe UI");
    Font nameFont(&fontFamily, 13, FontStyleBold, UnitPixel);
    Font statusFont(&fontFamily, 11, FontStyleRegular, UnitPixel);
    
    SolidBrush whiteBrush(Color(255, 255, 255));
    SolidBrush grayBrush(Color(255, 185, 187, 190)); // Серый Discord

    // Конвертация в WideChar для поддержки кириллицы
    int wlen = MultiByteToWideChar(CP_UTF8, 0, nameToDraw.c_str(), -1, NULL, 0);
    std::wstring wname(wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, nameToDraw.c_str(), -1, &wname[0], wlen);

    REAL textX = rectX + 12.0f + avatarSize + 10.0f;
    
    // Имя (чуть выше центра)
    g.DrawString(wname.c_str(), -1, &nameFont, PointF(textX, rectY + (rectH / 2.0f) - 16.0f), &whiteBrush);
    // Статус (чуть ниже центра)
    g.DrawString(L"В сети", -1, &statusFont, PointF(textX, rectY + (rectH / 2.0f) + 2.0f), &grayBrush);
}

bool IsClickOnProfile(int x, int y, int sidebarX, int windowHeight, int width) {
    int profileY = windowHeight - PROFILE_PANEL_HEIGHT;
    return (x >= sidebarX && x <= sidebarX + width && y >= profileY && y <= windowHeight);
}