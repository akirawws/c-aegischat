#ifndef SIDEBAR_H
#define SIDEBAR_H

#include <windows.h>

extern HWND hSidebar;

HWND CreateSidebar(HWND hParent, int x, int y, int width, int height);
void OnPaintSidebar(HDC hdc, int width, int height);
LRESULT CALLBACK SidebarWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

#endif

