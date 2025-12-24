#ifndef MAIN_PAGE_H
#define MAIN_PAGE_H

#include <windows.h>

extern HWND hMainWnd;

HWND CreateMainPage(HINSTANCE hInstance, int x, int y, int width, int height);
LRESULT CALLBACK MainWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

#endif

