#ifndef SIDEBAR_H
#define SIDEBAR_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>

extern HWND hSidebar;

HWND CreateSidebar(HWND hParent, int x, int y, int width, int height);

void OnPaintSidebar(HDC hdc, int width, int height);
LRESULT CALLBACK SidebarWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

#endif