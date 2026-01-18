#include "DeveloperBlog.h"
#include <gdiplus.h>
#include "Utils/Styles.h" 

using namespace Gdiplus;

void RegisterDeveloperBlogClass() {
    WNDCLASSA wc = {};
    wc.lpfnWndProc = DeveloperBlogProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "DeveloperBlogWindow";
    wc.hbrBackground = CreateSolidBrush(RGB(30, 30, 35)); 
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassA(&wc);
}

LRESULT CALLBACK DeveloperBlogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        
        Graphics g(hdc);
        SolidBrush brush(Color(255, 255, 255, 255));
        FontFamily fontFamily(L"Segoe UI");
        Font font(&fontFamily, 24, FontStyleBold, UnitPoint);
        g.DrawString(L"Developer Blog", -1, &font, PointF(20, 20), &brush);
        
        EndPaint(hwnd, &ps);
        return 0;
    }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}