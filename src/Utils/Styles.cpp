#include "Styles.h"
#include <cstring>
#include "Styles.h"

HFONT CreateAppFont(int size, int weight) {
    return CreateFontA(size, 0, 0, 0, weight, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, FONT_FAMILY);
}

HFONT CreateInputFont(int size, int weight) {
    HDC hdc = GetDC(NULL);
    int logPixels = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(NULL, hdc);
    
    int height = -MulDiv(size + 2, logPixels, 72);
    
    LOGFONTA lf = {};
    lf.lfHeight = height;
    lf.lfWeight = weight;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = CLEARTYPE_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    strcpy_s(lf.lfFaceName, FONT_FAMILY);
    
    return CreateFontIndirectA(&lf);
}
void DrawRoundedRect(HDC hdc, RECT r, COLORREF color, int radius) {
    HBRUSH brush = CreateSolidBrush(color);
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    
    // Выбираем кисть и перо в контекст устройства, запоминая старые
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);

    RoundRect(hdc, r.left, r.top, r.right, r.bottom, radius, radius);

    // Возвращаем старые объекты и удаляем созданные
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

