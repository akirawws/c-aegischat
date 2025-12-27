#ifndef AUTH_PAGE_H
#define AUTH_PAGE_H

#include <windows.h>


extern HWND hAuthWnd;
extern HWND hNameEdit;
extern HWND hEmailEdit;
extern HWND hPassEdit;
extern HWND hPassConfirmEdit;
extern HWND hRememberCheck;
extern HWND hActionBtn;
extern HWND hSwitchBtn;
HWND CreateAuthPage(HINSTANCE hInstance, int x, int y, int width, int height);
LRESULT CALLBACK AuthWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

#endif 
