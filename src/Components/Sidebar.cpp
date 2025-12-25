#include "Sidebar.h"
#include "Utils/Styles.h"
#include <string>
#include <vector>

HWND hSidebar = NULL;

// Активная страница
static int g_activeIndex = 0;
static int g_hoverIndex  = -1;

// ---------- Цветовая палитра (Discord style) ----------
constexpr COLORREF SIDEBAR_BG        = RGB(30, 31, 34);
constexpr COLORREF ITEM_BG           = RGB(54, 57, 63);
constexpr COLORREF ITEM_BG_HOVER     = RGB(64, 68, 75);
constexpr COLORREF ITEM_BG_ACTIVE    = RGB(88, 101, 242);
constexpr COLORREF DIVIDER_COLOR     = RGB(45, 47, 51);
constexpr COLORREF TEXT_WHITE        = RGB(255, 255, 255);

// ---------- Вспомогательное: скруглённый круг ----------
void DrawSmoothCircle(HDC hdc, int cx, int cy, int r, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    HPEN pen = CreatePen(PS_NULL, 0, 0);

    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);

    Ellipse(hdc, cx - r, cy - r, cx + r, cy + r);

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

// ---------- Рисование элемента ----------
void DrawSidebarItem(
    HDC hdc,
    int centerX,
    int centerY,
    const std::wstring& text,
    bool emoji,
    bool active,
    bool hover
) {
    COLORREF bg =
        active ? ITEM_BG_ACTIVE :
        hover  ? ITEM_BG_HOVER  :
                 ITEM_BG;

    DrawSmoothCircle(hdc, centerX, centerY, 22, bg);

    HFONT font;
    if (emoji) {
        font = CreateFontW(
            26, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE,
            L"Segoe UI Emoji"
        );
    } else {
        font = CreateAppFont(13, FW_BOLD);
    }

    HFONT oldFont = (HFONT)SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, TEXT_WHITE);

    RECT r = { centerX - 22, centerY - 22, centerX + 22, centerY + 22 };
    DrawTextW(hdc, text.c_str(), -1, &r,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    SelectObject(hdc, oldFont);
    DeleteObject(font);
}

// ---------- Отрисовка сайдбара ----------
void OnPaintSidebar(HDC hdc, int width, int height) {
    RECT bg = { 0, 0, width, height };
    HBRUSH bgBrush = CreateSolidBrush(SIDEBAR_BG);
    FillRect(hdc, &bg, bgBrush);
    DeleteObject(bgBrush);

    int centerX = width / 2;
    int y = 36;

    // --- Shield / Friends ---
    DrawSidebarItem(
        hdc,
        centerX,
        y,
        L"🛡",
        true,
        g_activeIndex == 0,
        g_hoverIndex == 0
    );

    // Divider
    HPEN pen = CreatePen(PS_SOLID, 1, DIVIDER_COLOR);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    MoveToEx(hdc, centerX - 12, y + 32, nullptr);
    LineTo(hdc, centerX + 12, y + 32);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);

    // --- Chats ---
    const wchar_t* chats[] = { L"gen", L"dev", L"news", L"off" };
    y += 60;

    for (int i = 0; i < 4; i++) {
        DrawSidebarItem(
            hdc,
            centerX,
            y,
            chats[i],
            false,
            g_activeIndex == (i + 1),
            g_hoverIndex == (i + 1)
        );
        y += 56;
    }
}

// ---------- Window ----------
HWND CreateSidebar(HWND hParent, int x, int y, int width, int height) {
    hSidebar = CreateWindowA(
        "SidebarWindow",
        "",
        WS_VISIBLE | WS_CHILD,
        x, y, width, height,
        hParent,
        nullptr,
        (HINSTANCE)GetWindowLongPtr(hParent, GWLP_HINSTANCE),
        nullptr
    );
    return hSidebar;
}

// ---------- Proc ----------
LRESULT CALLBACK SidebarWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_MOUSEMOVE: {
            int y = HIWORD(lParam);
            int index = -1;

            if (y >= 14 && y <= 58) index = 0;
            else if (y >= 96 && y <= 300)
                index = 1 + (y - 96) / 56;

            if (index != g_hoverIndex) {
                g_hoverIndex = index;
                InvalidateRect(hwnd, nullptr, TRUE);
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            int y = HIWORD(lParam);
            HWND hMain = GetParent(hwnd);

            if (y >= 14 && y <= 58) {
                g_activeIndex = 0;
                SendMessage(hMain, WM_USER + 2, 0, 0);
            } else if (y >= 96 && y <= 300) {
                g_activeIndex = 1 + (y - 96) / 56;
                SendMessage(hMain, WM_USER + 1, 0, 0);
            }

            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            RECT r;
            GetClientRect(hwnd, &r);

            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP bmp = CreateCompatibleBitmap(hdc, r.right, r.bottom);
            HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, bmp);

            OnPaintSidebar(memDC, r.right, r.bottom);

            BitBlt(hdc, 0, 0, r.right, r.bottom, memDC, 0, 0, SRCCOPY);

            SelectObject(memDC, oldBmp);
            DeleteObject(bmp);
            DeleteDC(memDC);

            EndPaint(hwnd, &ps);
            return 0;
        }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}
