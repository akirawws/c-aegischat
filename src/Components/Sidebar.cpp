#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <winsock2.h>
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <string>
#include <commctrl.h>
#include "Sidebar.h"
#include "Utils/Styles.h"

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "comctl32.lib")

using namespace Gdiplus;


HWND hSidebar = NULL;
Gdiplus::Image* g_pMainIcon = NULL; 
int g_activeIndex = 0;
int g_hoverIndex = -1;
static HWND hTooltip = NULL;
static bool g_mouseTracking = false;


const Color CLR_SIDEBAR_BG(255, 30, 31, 34);
const Color CLR_ITEM_BG(255, 49, 51, 56);
const Color CLR_BLURPLE(255, 88, 101, 242);
const Color CLR_GREEN(255, 35, 165, 89);
const Color CLR_WHITE(255, 255, 255);
const Color CLR_SEP(255, 45, 47, 51);

const std::wstring g_tooltips[] = { L"Главная", L"Developer Blog", L"Добавить сервер" };


HWND CreateSidebar(HWND hParent, int x, int y, int width, int height) {
    hSidebar = CreateWindowExA(0, "SidebarWindow", NULL,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        x, y, width, height, hParent, NULL, GetModuleHandle(NULL), NULL);
    return hSidebar;
}

void InitTooltip(HWND hwnd) {
    hTooltip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, NULL,
        WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        hwnd, NULL, GetModuleHandle(NULL), NULL);

    TOOLINFOW ti = { 0 };
    ti.cbSize = sizeof(ti);
    ti.uFlags = TTF_IDISHWND; 
    ti.hwnd = hwnd;
    ti.uId = (UINT_PTR)hwnd;
    ti.lpszText = (LPWSTR)L"";
    
    SendMessageW(hTooltip, TTM_ADDTOOLW, 0, (LPARAM)&ti);
    SendMessageW(hTooltip, TTM_ACTIVATE, TRUE, 0);
    SendMessageW(hTooltip, TTM_SETMAXTIPWIDTH, 0, 300);
}

void RelayToTooltip(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (hTooltip) {
        MSG msg;
        msg.hwnd = hwnd;
        msg.message = uMsg;
        msg.wParam = wParam;
        msg.lParam = lParam;
        SendMessageW(hTooltip, TTM_RELAYEVENT, 0, (LPARAM)&msg);
    }
}

void UpdateTooltipText(HWND hwnd, const std::wstring& text) {
    if (!hTooltip) return;
    TOOLINFOW ti = { 0 };
    ti.cbSize = sizeof(ti);
    ti.hwnd = hwnd;
    ti.uId = (UINT_PTR)hwnd;
    ti.lpszText = (LPWSTR)text.c_str();
    SendMessageW(hTooltip, TTM_UPDATETIPTEXTW, 0, (LPARAM)&ti);
}

void DrawSidebarItem(Graphics& g, int centerX, int centerY, const std::wstring& text, bool isPlus, bool active, bool hover, Gdiplus::Image* pImg = NULL) {
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic); 
    
    Color bgColor = CLR_ITEM_BG;
    if (isPlus) {
        if (hover) bgColor = CLR_GREEN;
    } else {
        if (active || hover) bgColor = CLR_BLURPLE;
    }

    SolidBrush brush(bgColor);
    float size = 44.0f;
    float x = (float)centerX - size / 2;
    float y = (float)centerY - size / 2;

    if (active || hover) {
        GraphicsPath path;
        float r = 14.0f; 
        path.AddArc(x, y, r, r, 180, 90);
        path.AddArc(x + size - r, y, r, r, 270, 90);
        path.AddArc(x + size - r, y + size - r, r, r, 0, 90);
        path.AddArc(x, y + size - r, r, r, 90, 90);
        path.CloseFigure();
        g.FillPath(&brush, &path);
    } else {
        g.FillEllipse(&brush, x, y, size, size);
    }

    if (isPlus) {
        Pen plusPen(hover ? CLR_WHITE : CLR_GREEN, 2.0f);
        g.DrawLine(&plusPen, centerX - 8, centerY, centerX + 8, centerY);
        g.DrawLine(&plusPen, centerX, centerY - 8, centerX, centerY + 8);
    } 
    else if (pImg) {
        float padding = 10.0f;
        float imgSize = size - padding * 2;
        g.DrawImage(pImg, x + padding, y + padding, imgSize, imgSize);
    }
    else {
        FontFamily fontFamily(L"Segoe UI");
        Gdiplus::Font font(&fontFamily, 11, FontStyleBold, UnitPoint);
        SolidBrush textBrush(CLR_WHITE);
        StringFormat format;
        format.SetAlignment(StringAlignmentCenter);
        format.SetLineAlignment(StringAlignmentCenter);
        RectF rect(x, y, size, size);
        g.DrawString(text.c_str(), -1, &font, rect, &format, &textBrush);
    }
}

void OnPaintSidebar(HDC hdc, int width, int height) {
    Graphics g(hdc);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    g.Clear(CLR_SIDEBAR_BG);

    int centerX = width / 2;
    int y = 36;

    DrawSidebarItem(g, centerX, y, L"", false, g_activeIndex == 0, g_hoverIndex == 0, g_pMainIcon);
    
    y += 66; 
    DrawSidebarItem(g, centerX, y, L"dev", false, g_activeIndex == 1, g_hoverIndex == 1, NULL);
    
    y += 56;
    DrawSidebarItem(g, centerX, y, L"", true, false, g_hoverIndex == 2, NULL);
}

LRESULT CALLBACK SidebarWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg >= WM_MOUSEFIRST && uMsg <= WM_MOUSELAST) {
        RelayToTooltip(hwnd, uMsg, wParam, lParam);
    }

    switch (uMsg) {
    case WM_CREATE:
        InitTooltip(hwnd);
        g_pMainIcon = Gdiplus::Image::FromFile(L"assets/icon.png");
        return 0;

    case WM_MOUSEMOVE: {
        if (!g_mouseTracking) {
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, HOVER_DEFAULT };
            TrackMouseEvent(&tme);
            g_mouseTracking = true;
        }
        int yPos = HIWORD(lParam);
        int oldHover = g_hoverIndex;

        if (yPos >= 14 && yPos <= 58) g_hoverIndex = 0;
        else if (yPos >= 80 && yPos <= 124) g_hoverIndex = 1;
        else if (yPos >= 136 && yPos <= 180) g_hoverIndex = 2;
        else g_hoverIndex = -1;

        if (oldHover != g_hoverIndex) {
            if (g_hoverIndex != -1) {
                UpdateTooltipText(hwnd, g_tooltips[g_hoverIndex]);
                SendMessageW(hTooltip, TTM_ACTIVATE, TRUE, 0); 
            }
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_MOUSELEAVE:
        g_hoverIndex = -1;
        g_mouseTracking = false;
        UpdateTooltipText(hwnd, L"");
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_LBUTTONDOWN: {
        int yPos = HIWORD(lParam);
        if (yPos >= 14 && yPos <= 58) g_activeIndex = 0;
        else if (yPos >= 80 && yPos <= 124) g_activeIndex = 1;
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP hBmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        HGDIOBJ oldBmp = SelectObject(memDC, hBmp);

        OnPaintSidebar(memDC, rc.right, rc.bottom);

        BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);
        SelectObject(memDC, oldBmp);
        DeleteObject(hBmp);
        DeleteDC(memDC);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        if (g_pMainIcon) {
            delete g_pMainIcon;
            g_pMainIcon = NULL;
        }
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}