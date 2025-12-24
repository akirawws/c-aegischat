#include "ConnectPage.h"
#include "Utils/Styles.h"
#include "Utils/Utils.h"
#include "Utils/Network.h"
#include <cstring>

HWND hConnectWnd = NULL;
HWND hIPEdit = NULL;
HWND hPortEdit = NULL;
HWND hNameEdit = NULL;
HWND hConnectBtn = NULL;

extern HWND hMainWnd;

HWND CreateConnectPage(HINSTANCE hInstance, int x, int y, int width, int height) {
    hConnectWnd = CreateWindowExA(0, "ConnectWindow", "AEGIS - Connection",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        x, y, width, height,
        NULL, NULL, hInstance, NULL);
    return hConnectWnd;
}

LRESULT CALLBACK ConnectWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        int fieldW = 300;
        int fieldH = 35;
        int startX = (420 - fieldW) / 2 - 10;

        hIPEdit = CreateWindowA("EDIT", "xisyrurdm.localto.net",
            WS_VISIBLE | WS_CHILD | ES_LEFT | WS_BORDER | ES_AUTOHSCROLL,
            startX, 120, fieldW, fieldH, hwnd, NULL, NULL, NULL);

        hPortEdit = CreateWindowA("EDIT", "6162",
            WS_VISIBLE | WS_CHILD | ES_LEFT | WS_BORDER | ES_AUTOHSCROLL,
            startX, 185, fieldW, fieldH, hwnd, NULL, NULL, NULL);

        hNameEdit = CreateWindowA("EDIT", "",
            WS_VISIBLE | WS_CHILD | ES_LEFT | WS_BORDER | ES_AUTOHSCROLL,
            startX, 250, fieldW, fieldH, hwnd, NULL, NULL, NULL);

        hConnectBtn = CreateWindowA("BUTTON", "Connect",
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 
            startX, 320, fieldW, 45, hwnd, (HMENU)1, NULL, NULL);
        
        HFONT hEditFont = CreateAppFont(20, FONT_WEIGHT_NORMAL);
        SendMessage(hIPEdit, WM_SETFONT, (WPARAM)hEditFont, TRUE);
        SendMessage(hPortEdit, WM_SETFONT, (WPARAM)hEditFont, TRUE);
        SendMessage(hNameEdit, WM_SETFONT, (WPARAM)hEditFont, TRUE);
        break;
    }

    case WM_DRAWITEM: {
        if (wParam == 1) {
            LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
            HDC hdc = dis->hDC;
            RECT rect = dis->rcItem;

            HBRUSH hBtnBrush = CreateSolidBrush((dis->itemState & ODS_SELECTED) ? 
                               COLOR_ACCENT_BLUE_DARK : COLOR_ACCENT_BLUE);
            
            FillRect(hdc, &rect, hBtnBrush);
            DeleteObject(hBtnBrush);

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, COLOR_TEXT_WHITE);
            HFONT hBtnFont = CreateAppFont(18, FONT_WEIGHT_BOLD);
            HFONT hOldFont = (HFONT)SelectObject(hdc, hBtnFont);
            
            DrawTextA(hdc, "Connect", -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            
            SelectObject(hdc, hOldFont);
            DeleteObject(hBtnFont);
            return TRUE;
        }
        break;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == 1) { 
            char ip[256], port[256], name[256];
            GetWindowTextA(hIPEdit, ip, 256);
            GetWindowTextA(hPortEdit, port, 256);
            GetWindowTextA(hNameEdit, name, 256);
            
            if (strlen(ip) == 0 || strlen(port) == 0 || strlen(name) == 0) {
                MessageBoxW(hwnd, L"Заполните все поля", L"Ошибка", MB_OK | MB_ICONERROR);
                break;
            }
            
            userName = name;
            userAvatar = GetAvatar(userName);
            WriteLog("Connect button clicked. Target: " + std::string(ip) + ":" + std::string(port));

            if (ConnectToServer(ip, port)) {
                ShowWindow(hwnd, SW_HIDE);
                ShowWindow(hMainWnd, SW_SHOW);
            } else {
                int lastError = WSAGetLastError();
                MessageBoxW(hwnd, L"Ошибка подключения! Проверьте данные.", L"Error", MB_OK | MB_ICONERROR);
            }
        }
        break;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rect;
        GetClientRect(hwnd, &rect);
        HBRUSH brush = CreateSolidBrush(COLOR_BG_DARK);
        FillRect(hdc, &rect, brush);
        DeleteObject(brush);
        
        SetTextColor(hdc, COLOR_TEXT_WHITE);
        SetBkMode(hdc, TRANSPARENT);
        HFONT hFontLog = CreateAppFont(FONT_SIZE_LARGE, FONT_WEIGHT_BOLD);
        SelectObject(hdc, hFontLog);
        RECT textRect = { 0, 32, rect.right, 72 };
        DrawTextA(hdc, "AEGIS", -1, &textRect, DT_CENTER | DT_VCENTER);
        DeleteObject(hFontLog);

        HFONT hLabelFont = CreateAppFont(14, FONT_WEIGHT_NORMAL);
        SelectObject(hdc, hLabelFont);
        SetTextColor(hdc, COLOR_TEXT_GRAY);
        
        textRect = { 55, 100, 350, 120 }; DrawTextA(hdc, "Server IP address", -1, &textRect, DT_LEFT);
        textRect = { 55, 165, 350, 185 }; DrawTextA(hdc, "Port", -1, &textRect, DT_LEFT);
        textRect = { 55, 230, 350, 250 }; DrawTextA(hdc, "Your name", -1, &textRect, DT_LEFT);
        
        DeleteObject(hLabelFont);
        EndPaint(hwnd, &ps);
        break;
    }

    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wParam;
        SetBkColor(hdc, COLOR_INPUT_BG);
        SetTextColor(hdc, COLOR_TEXT_WHITE);
        static HBRUSH hEditBg = CreateSolidBrush(COLOR_INPUT_BG);
        return (LRESULT)hEditBg;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

