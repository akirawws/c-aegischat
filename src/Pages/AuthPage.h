#ifndef AUTH_PAGE_H
#define AUTH_PAGE_H

#include <windows.h>

// Главное окно авторизации
extern HWND hAuthWnd;

// Поля ввода
extern HWND hNameEdit;
extern HWND hEmailEdit;
extern HWND hPassEdit;
extern HWND hPassConfirmEdit;

// Кнопки и чекбоксы
extern HWND hRememberCheck;
extern HWND hActionBtn;
extern HWND hSwitchBtn;

// Создание страницы авторизации
HWND CreateAuthPage(HINSTANCE hInstance, int x, int y, int width, int height);

// Процедура окна авторизации
LRESULT CALLBACK AuthWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

#endif // AUTH_PAGE_H
