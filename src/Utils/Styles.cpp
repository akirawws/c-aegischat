#include "Styles.h"
#include <cstring>

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

