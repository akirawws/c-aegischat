#include "AuthPage.h"
#include "Utils/Styles.h"
#include "Utils/Utils.h"
#include "Utils/Network.h"
#include "Utils/AuthProtocol.h"
#include "Utils/HashPassword.h"
#include "Utils/ConfigUtils.h"
#include "Pages/MainPage.h"
#include <cstring>
#include <string>
#include <wingdi.h>
#include <windowsx.h>
#include <commctrl.h> // ← для WM_CTLCOLOREDIT

#pragma comment(lib, "msimg32.lib")

enum AuthState { STATE_LOGIN, STATE_REGISTER };
AuthState currentAuthState = STATE_LOGIN;

HWND hAuthWnd = NULL;
HWND hNameEdit = NULL;
HWND hEmailEdit = NULL;
HWND hPassEdit = NULL;
HWND hPassConfirmEdit = NULL;
HWND hRememberCheck = NULL;
HWND hRememberLabel = NULL;
HWND hActionBtn = NULL;
HWND hSwitchBtn = NULL;
HWND hQuickLoginBtn = NULL;

std::string placeholderName = "Login";
std::string placeholderEmail = "Email";
std::string placeholderPass = "Password";
std::string placeholderPassConf = "Repeat password";

extern HWND hMainWnd;

void SetRoundedCorners(HWND hwnd, int radius = 12) {
    RECT rect;
    GetWindowRect(hwnd, &rect);
    OffsetRect(&rect, -rect.left, -rect.top);
    HRGN hRgn = CreateRoundRectRgn(0, 0, rect.right, rect.bottom, radius, radius);
    SetWindowRgn(hwnd, hRgn, TRUE);
}

void ToggleAuthMode(HWND hwnd) {
    if (currentAuthState == STATE_LOGIN) {
        ShowWindow(hEmailEdit, SW_HIDE);
        ShowWindow(hPassConfirmEdit, SW_HIDE);
        ShowWindow(hRememberCheck, SW_SHOW);
        ShowWindow(hRememberLabel, SW_SHOW);
        SetWindowTextA(hActionBtn, "Login");
        SetWindowTextA(hSwitchBtn, "Don't have an account? Register");
    } else {
        ShowWindow(hEmailEdit, SW_SHOW);
        ShowWindow(hPassConfirmEdit, SW_SHOW);
        ShowWindow(hRememberCheck, SW_HIDE);
        ShowWindow(hRememberLabel, SW_HIDE);
        SetWindowTextA(hActionBtn, "Create Account");
        SetWindowTextA(hSwitchBtn, "Already have an account? Login");
    }
    InvalidateRect(hwnd, NULL, TRUE);
}

HWND CreateAuthPage(HINSTANCE hInstance, int x, int y, int width, int height) {
    WNDCLASSA wc = {};
    wc.lpfnWndProc = AuthWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "AuthWindow";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(255, 255, 255)); // ← белый фон
    RegisterClassA(&wc);

    hAuthWnd = CreateWindowA(
        "AuthWindow", "AEGIS - Authorization",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        x, y, width, 600, NULL, NULL, hInstance, NULL
    );
    return hAuthWnd;
}

static bool g_bActionBtnHover = false;
static bool g_bSwitchBtnHover = false;
static bool g_bQuickLoginBtnHover = false;

LRESULT CALLBACK AuthWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        int fieldW = 320, fieldH = 38;
        int startX = (420 - fieldW) / 2;

        hNameEdit = CreateWindowA("EDIT", "", WS_VISIBLE | WS_CHILD | ES_LEFT | WS_BORDER | ES_AUTOHSCROLL,
            startX, 150, fieldW, fieldH, hwnd, NULL, NULL, NULL);
        hEmailEdit = CreateWindowA("EDIT", "", WS_CHILD | ES_LEFT | WS_BORDER | ES_AUTOHSCROLL,
            startX, 210, fieldW, fieldH, hwnd, NULL, NULL, NULL);
        hPassEdit = CreateWindowA("EDIT", "", WS_VISIBLE | WS_CHILD | ES_LEFT | WS_BORDER | ES_PASSWORD,
            startX, 270, fieldW, fieldH, hwnd, NULL, NULL, NULL);
        hPassConfirmEdit = CreateWindowA("EDIT", "", WS_CHILD | ES_LEFT | WS_BORDER | ES_PASSWORD,
            startX, 330, fieldW, fieldH, hwnd, NULL, NULL, NULL);

        SetWindowTextA(hNameEdit, placeholderName.c_str());
        SetWindowTextA(hEmailEdit, placeholderEmail.c_str());
        SetWindowTextA(hPassEdit, placeholderPass.c_str());
        SetWindowTextA(hPassConfirmEdit, placeholderPassConf.c_str());

        hRememberCheck = CreateWindowA("BUTTON", "",
            WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
            startX, 375, 13, 13, hwnd, NULL, NULL, NULL);
        hRememberLabel = CreateWindowA("STATIC", " Remember me on this device",
            WS_VISIBLE | WS_CHILD | SS_LEFT,
            startX + 25, 375, fieldW - 25, 20, hwnd, NULL, NULL, NULL);

        hActionBtn = CreateWindowA("BUTTON", "Login", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
            startX, 405, fieldW, 45, hwnd, (HMENU)1, NULL, NULL);
        hSwitchBtn = CreateWindowA("BUTTON", "Don't have an account? Register",
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
            startX, 465, fieldW, 28, hwnd, (HMENU)2, NULL, NULL);
        hQuickLoginBtn = CreateWindowA("BUTTON",
            "Do you already have an account? Log in to it?",
            WS_CHILD | BS_OWNERDRAW,
            startX, 500, fieldW, 28, hwnd, (HMENU)3, NULL, NULL);
        ShowWindow(hQuickLoginBtn, SW_HIDE);

        HFONT hFont = CreateAppFont(16, FONT_WEIGHT_NORMAL);
        SendMessage(hNameEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(hEmailEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(hPassEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(hPassConfirmEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(hRememberCheck, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(hRememberLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(hSwitchBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(hQuickLoginBtn, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Округление
        SetRoundedCorners(hNameEdit, 8);
        SetRoundedCorners(hEmailEdit, 8);
        SetRoundedCorners(hPassEdit, 8);
        SetRoundedCorners(hPassConfirmEdit, 8);
        SetRoundedCorners(hActionBtn, 12);
        SetRoundedCorners(hSwitchBtn, 10);
        SetRoundedCorners(hQuickLoginBtn, 10);

        if (ConnectToServer("xisyrurdm.localto.net", "6162")) {
            std::string savedToken = ReadSessionToken();
            if (!savedToken.empty())
                ShowWindow(hQuickLoginBtn, SW_SHOW);
        }

        ToggleAuthMode(hwnd);
        break;
    }

    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        HWND hCtrl = (HWND)lParam;

        if (HIWORD(wParam) == EN_SETFOCUS) {
            char text[256]; GetWindowTextA(hCtrl, text, 256);
            if (hCtrl == hNameEdit && strcmp(text, placeholderName.c_str()) == 0) SetWindowTextA(hCtrl, "");
            if (hCtrl == hEmailEdit && strcmp(text, placeholderEmail.c_str()) == 0) SetWindowTextA(hCtrl, "");
            if (hCtrl == hPassEdit && strcmp(text, placeholderPass.c_str()) == 0) SetWindowTextA(hCtrl, "");
            if (hCtrl == hPassConfirmEdit && strcmp(text, placeholderPassConf.c_str()) == 0) SetWindowTextA(hCtrl, "");
        }
        if (HIWORD(wParam) == EN_KILLFOCUS) {
            char text[256]; GetWindowTextA(hCtrl, text, 256);
            if (hCtrl == hNameEdit && strlen(text) == 0) SetWindowTextA(hCtrl, placeholderName.c_str());
            if (hCtrl == hEmailEdit && strlen(text) == 0) SetWindowTextA(hCtrl, placeholderEmail.c_str());
            if (hCtrl == hPassEdit && strlen(text) == 0) SetWindowTextA(hCtrl, placeholderPass.c_str());
            if (hCtrl == hPassConfirmEdit && strlen(text) == 0) SetWindowTextA(hCtrl, placeholderPassConf.c_str());
        }

        if (wmId == 2) {
            currentAuthState = (currentAuthState == STATE_LOGIN) ? STATE_REGISTER : STATE_LOGIN;
            ToggleAuthMode(hwnd);
        }

        if (wmId == 3) {
            std::string token = ReadSessionToken();
            if (token.empty()) {
                MessageBoxA(hwnd, "No saved session found.", "Info", MB_OK | MB_ICONINFORMATION);
                break;
            }
            if (!ConnectToServer("xisyrurdm.localto.net", "6162")) {
                MessageBoxA(hwnd, "Server not responding!", "Error", MB_OK | MB_ICONERROR);
                break;
            }

            TokenAuthPacket packet = { 0 };
            packet.type = PACKET_TOKEN_AUTH;
            strncpy(packet.token, token.c_str(), sizeof(packet.token) - 1);
            if (!SendPacket((char*)&packet, sizeof(packet))) {
                MessageBoxA(hwnd, "Quick login failed.", "Error", MB_OK | MB_ICONERROR);
                break;
            }

            ResponsePacket response = { 0 };
            if (!ReceivePacket((char*)&response, sizeof(response)) || !response.success) {
                MessageBoxA(hwnd, "Session expired. Please login again.", "Error", MB_OK | MB_ICONERROR);
                ClearSessionToken();
                ShowWindow(hQuickLoginBtn, SW_HIDE);
                break;
            }

            userName = response.message;
            StartMessageSystem();
            WriteLog("Quick login success: " + userName);

            HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
            int sw = GetSystemMetrics(SM_CXSCREEN);
            int sh = GetSystemMetrics(SM_CYSCREEN);
            int ww = 1200, wh = 700;
            int x = (sw - ww) / 2, y = (sh - wh) / 2;
            hMainWnd = CreateMainPage(hInstance, x, y, ww, wh);
            ShowWindow(hwnd, SW_HIDE);
            ShowWindow(hMainWnd, SW_SHOW);
            UpdateWindow(hMainWnd);
            return 0;
        }

        if (wmId == 1) {
            char name[256], email[256], pass[256], passConf[256];
            GetWindowTextA(hNameEdit, name, 256);
            GetWindowTextA(hPassEdit, pass, 256);

            if (currentAuthState == STATE_REGISTER) {
                GetWindowTextA(hEmailEdit, email, 256);
                GetWindowTextA(hPassConfirmEdit, passConf, 256);
                if (strlen(name) < 3 || strlen(pass) < 6) {
                    MessageBoxW(hwnd, L"Name > 3, Password > 6 characters", L"Error", MB_ICONERROR);
                    break;
                }
                if (strcmp(pass, passConf) != 0) {
                    MessageBoxW(hwnd, L"Passwords do not match", L"Error", MB_ICONERROR);
                    break;
                }
            }

            std::string hashed = HashPassword(pass, name);
            AuthPacket packet = { 0 };
            packet.type = (currentAuthState == STATE_REGISTER) ? PACKET_REGISTER : PACKET_LOGIN;
            strncpy(packet.username, name, sizeof(packet.username) - 1);
            strncpy(packet.password, hashed.c_str(), sizeof(packet.password) - 1);
            if (currentAuthState == STATE_REGISTER)
                GetWindowTextA(hEmailEdit, packet.email, sizeof(packet.email) - 1);

            packet.rememberMe = (SendMessage(hRememberCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);

            if (!SendPacket((char*)&packet, sizeof(packet))) {
                MessageBoxW(hwnd, L"Failed to send data.", L"Network error", MB_OK | MB_ICONERROR);
                break;
            }

            ResponsePacket resp = { 0 };
            if (!ReceivePacket((char*)&resp, sizeof(resp))) {
                MessageBoxW(hwnd, L"Server not responding.", L"Error", MB_OK | MB_ICONERROR);
                break;
            }

            if (!resp.success) {
                MessageBoxA(hwnd, resp.message, "Auth Error", MB_OK | MB_ICONERROR);
                break;
            }

            if (currentAuthState == STATE_REGISTER) {
                MessageBoxA(hwnd, resp.message, "Success", MB_OK | MB_ICONINFORMATION);
                currentAuthState = STATE_LOGIN;
                ToggleAuthMode(hwnd);
                break;
            }

            userName = name;
            if (packet.rememberMe) {
                std::string token(resp.message, 16);
                SaveSessionToken(token);
                WriteLog("Session token saved for user: " + userName);
                ShowWindow(hQuickLoginBtn, SW_SHOW);
            } else {
                ClearSessionToken();
                ShowWindow(hQuickLoginBtn, SW_HIDE);
            }

            StartMessageSystem();
            WriteLog("User logged in: " + userName);

            HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
            int sw = GetSystemMetrics(SM_CXSCREEN);
            int sh = GetSystemMetrics(SM_CYSCREEN);
            int ww = 1200, wh = 700;
            int x = (sw - ww) / 2, y = (sh - wh) / 2;
            hMainWnd = CreateMainPage(hInstance, x, y, ww, wh);
            ShowWindow(hwnd, SW_HIDE);
            ShowWindow(hMainWnd, SW_SHOW);
            UpdateWindow(hMainWnd);
        }
        break;
    }

    case WM_MOUSEMOVE: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

        auto CheckHover = [&](HWND btn, bool& flag, const RECT& rc) {
            if (PtInRect(&rc, pt)) {
                if (!flag) { flag = true; InvalidateRect(btn, NULL, FALSE); }
            } else {
                if (flag) { flag = false; InvalidateRect(btn, NULL, FALSE); }
            }
        };

        RECT rc;
        GetWindowRect(hActionBtn, &rc); ScreenToClient(hwnd, (LPPOINT)&rc.left); ScreenToClient(hwnd, (LPPOINT)&rc.right);
        CheckHover(hActionBtn, g_bActionBtnHover, rc);

        GetWindowRect(hSwitchBtn, &rc); ScreenToClient(hwnd, (LPPOINT)&rc.left); ScreenToClient(hwnd, (LPPOINT)&rc.right);
        CheckHover(hSwitchBtn, g_bSwitchBtnHover, rc);

        GetWindowRect(hQuickLoginBtn, &rc); ScreenToClient(hwnd, (LPPOINT)&rc.left); ScreenToClient(hwnd, (LPPOINT)&rc.right);
        CheckHover(hQuickLoginBtn, g_bQuickLoginBtnHover, rc);
        break;
    }

    case WM_MOUSELEAVE:
        g_bActionBtnHover = false;
        g_bSwitchBtnHover = false;
        g_bQuickLoginBtnHover = false;
        InvalidateRect(hActionBtn, NULL, FALSE);
        InvalidateRect(hSwitchBtn, NULL, FALSE);
        InvalidateRect(hQuickLoginBtn, NULL, FALSE);
        break;

    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
        HDC hdc = dis->hDC;
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));

        COLORREF baseColor = RGB(180, 30, 30);
        COLORREF hoverColor = RGB(200, 40, 40);
        COLORREF pressedColor = RGB(150, 20, 20);

        if (dis->CtlID == 1) { // ActionBtn
            COLORREF color;
            if (dis->itemState & ODS_SELECTED) {
                color = pressedColor;
            } else if (g_bActionBtnHover) {
                color = hoverColor;
            } else {
                color = baseColor;
            }

            HBRUSH brush = CreateSolidBrush(color);
            HRGN rgn = CreateRoundRectRgn(dis->rcItem.left, dis->rcItem.top, dis->rcItem.right, dis->rcItem.bottom, 12, 12);
            FillRgn(hdc, rgn, brush);
            DeleteObject(rgn);
            DeleteObject(brush);

            HFONT hFont = CreateAppFont(17, FONT_WEIGHT_BOLD);
            HFONT hOld = (HFONT)SelectObject(hdc, hFont);
            DrawTextA(hdc, (currentAuthState == STATE_LOGIN) ? "Login" : "Create Account", -1,
                &dis->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdc, hOld);
            DeleteObject(hFont);
            return TRUE;
        }

        if (dis->CtlID == 2 || dis->CtlID == 3) {
            COLORREF color;
            if (dis->itemState & ODS_SELECTED) {
                color = RGB(130, 20, 20);
            } else if ((dis->CtlID == 2 && g_bSwitchBtnHover) || (dis->CtlID == 3 && g_bQuickLoginBtnHover)) {
                color = RGB(160, 30, 30);
            } else {
                color = baseColor;
            }

            HBRUSH brush = CreateSolidBrush(color);
            HRGN rgn = CreateRoundRectRgn(dis->rcItem.left, dis->rcItem.top, dis->rcItem.right, dis->rcItem.bottom, 10, 10);
            FillRgn(hdc, rgn, brush);
            DeleteObject(rgn);
            DeleteObject(brush);

            HFONT hFont = CreateAppFont(16, FONT_WEIGHT_NORMAL);
            HFONT hOld = (HFONT)SelectObject(hdc, hFont);
            char text[256];
            GetWindowTextA((HWND)dis->hwndItem, text, 256);
            DrawTextA(hdc, text, -1, &dis->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdc, hOld);
            DeleteObject(hFont);
            return TRUE;
        }
        break;
    }

    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)lParam;
        SetBkColor(hdc, RGB(60, 10, 10));       // ← фон поля — тёмно-красный
        SetTextColor(hdc, RGB(255, 255, 255));   // ← текст — белый
        SetBkMode(hdc, TRANSPARENT);
        return (INT_PTR)CreateSolidBrush(RGB(60, 10, 10));
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)lParam;
        SetTextColor(hdc, RGB(0, 0, 0)); // ← "Remember me" — чёрный на белом фоне
        SetBkMode(hdc, TRANSPARENT);
        return (INT_PTR)GetStockObject(NULL_BRUSH);
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(0, 0, 0));

        RECT r; GetClientRect(hwnd, &r);
        HBRUSH bg = CreateSolidBrush(RGB(255, 255, 255));
        FillRect(hdc, &r, bg);
        DeleteObject(bg);

        HFONT hTitle = CreateAppFont(30, FONT_WEIGHT_BOLD);
        HFONT hOld = (HFONT)SelectObject(hdc, hTitle);
        RECT tRect = {0, 50, r.right, 90};
        SetTextColor(hdc, RGB(0, 0, 0));
        DrawTextA(hdc, "Welcome to Aegis", -1, &tRect, DT_CENTER);
        SelectObject(hdc, hOld);
        DeleteObject(hTitle);
        EndPaint(hwnd, &ps);
        break;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}