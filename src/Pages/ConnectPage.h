#ifndef CONNECT_PAGE_H
#define CONNECT_PAGE_H

#include <windows.h>

extern HWND hConnectWnd;
extern HWND hIPEdit;
extern HWND hPortEdit;
extern HWND hNameEdit;
extern HWND hConnectBtn;

HWND CreateConnectPage(HINSTANCE hInstance, int x, int y, int width, int height);
LRESULT CALLBACK ConnectWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

#endif

