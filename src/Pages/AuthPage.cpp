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
#pragma comment(lib, "msimg32.lib")


enum AuthState { STATE_LOGIN, STATE_REGISTER };
AuthState currentAuthState = STATE_LOGIN;

HWND hAuthWnd = NULL;
HWND hNameEdit = NULL;     
HWND hEmailEdit = NULL;    
HWND hPassEdit = NULL;
HWND hPassConfirmEdit = NULL;
HWND hRememberCheck = NULL;
HWND hActionBtn = NULL;    
HWND hSwitchBtn = NULL;    
const char* PLACEHOLDER_NAME = "Username";
const char* PLACEHOLDER_EMAIL = "Email";
const char* PLACEHOLDER_PASS = "Password";
const char* PLACEHOLDER_PASS_CONFIRM = "Confirm Password";

bool isNamePlaceholder = true;
bool isEmailPlaceholder = true;
bool isPassPlaceholder = true;
bool isPassConfirmPlaceholder = true;

HWND hNameLabel = NULL;
HWND hEmailLabel = NULL;
HWND hPassLabel = NULL;
HWND hPassConfirmLabel = NULL;
#define COLOR_BUTTON RGB(180, 70, 80)     
#define COLOR_BUTTON_HOVER RGB(200, 90, 100) 
#define COLOR_BUTTON_ACTIVE RGB(160, 50, 60)

extern HWND hMainWnd;

void ToggleAuthMode(HWND hwnd) {
    if (currentAuthState == STATE_LOGIN) {
        ShowWindow(hEmailEdit, SW_HIDE);
        ShowWindow(hPassConfirmEdit, SW_HIDE);
        ShowWindow(hEmailLabel, SW_HIDE);
        ShowWindow(hPassConfirmLabel, SW_HIDE);

        ShowWindow(hRememberCheck, SW_SHOW);
        SetWindowTextA(hActionBtn, "Login");
        SetWindowTextA(hSwitchBtn, "Don't have an account? Register");
    } else {
        ShowWindow(hEmailEdit, SW_SHOW);
        ShowWindow(hPassConfirmEdit, SW_SHOW);
        ShowWindow(hEmailLabel, SW_SHOW);
        ShowWindow(hPassConfirmLabel, SW_SHOW);

        ShowWindow(hRememberCheck, SW_HIDE);
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
    wc.hbrBackground = CreateSolidBrush(COLOR_BG_DARK);

    RegisterClassA(&wc);

    hAuthWnd = CreateWindowA(
        "AuthWindow",
        "AEGIS — Authorization",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        x, y, width, 580,   
        NULL,
        NULL,
        hInstance,
        NULL
    );

    return hAuthWnd;
}

LRESULT CALLBACK AuthWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        int fieldW = 320;
        int fieldH = 40;
        int startX = (420 - fieldW) / 2;
        int labelYOffset = -20;

        // --- Создание меток (Labels) ---
        hNameLabel = CreateWindowA("STATIC", "Username", WS_VISIBLE | WS_CHILD | SS_LEFT,
            startX, 130 + labelYOffset, fieldW, 20, hwnd, NULL, NULL, NULL);

        hEmailLabel = CreateWindowA("STATIC", "Email", WS_CHILD | WS_VISIBLE | SS_LEFT,
            startX, 200 + labelYOffset, fieldW, 20, hwnd, NULL, NULL, NULL);

        hPassLabel = CreateWindowA("STATIC", "Password", WS_VISIBLE | WS_CHILD | SS_LEFT,
            startX, 270 + labelYOffset, fieldW, 20, hwnd, NULL, NULL, NULL);

        hPassConfirmLabel = CreateWindowA("STATIC", "Confirm Password", WS_CHILD | WS_VISIBLE | SS_LEFT,
            startX, 340 + labelYOffset, fieldW, 20, hwnd, NULL, NULL, NULL);

        // --- Создание полей ввода (Edits) ---
        hNameEdit = CreateWindowA("EDIT", PLACEHOLDER_NAME, WS_VISIBLE | WS_CHILD | ES_LEFT | WS_BORDER | ES_AUTOHSCROLL,
            startX, 130, fieldW, fieldH, hwnd, NULL, NULL, NULL);

        hEmailEdit = CreateWindowA("EDIT", PLACEHOLDER_EMAIL, WS_CHILD | ES_LEFT | WS_BORDER | ES_AUTOHSCROLL,
            startX, 200, fieldW, fieldH, hwnd, NULL, NULL, NULL);

        hPassEdit = CreateWindowA("EDIT", PLACEHOLDER_PASS, WS_VISIBLE | WS_CHILD | ES_LEFT | WS_BORDER | ES_PASSWORD,
            startX, 270, fieldW, fieldH, hwnd, NULL, NULL, NULL);

        hPassConfirmEdit = CreateWindowA("EDIT", PLACEHOLDER_PASS_CONFIRM, WS_CHILD | ES_LEFT | WS_BORDER | ES_PASSWORD,
            startX, 340, fieldW, fieldH, hwnd, NULL, NULL, NULL);

        // --- Создание кнопок (Buttons) ---
        hRememberCheck = CreateWindowA("BUTTON", "Remember me on this device",
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
            startX, 320, fieldW, 25, hwnd, (HMENU)3, NULL, NULL);

        hActionBtn = CreateWindowA("BUTTON", "Login",
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
            startX, 380, fieldW, 48, hwnd, (HMENU)1, NULL, NULL);

        hSwitchBtn = CreateWindowA("BUTTON", "Don't have an account? Register",
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
            startX, 440, fieldW, 30, hwnd, (HMENU)2, NULL, NULL);

        // --- Установка шрифтов ---
        HFONT hEditFont = CreateAppFont(16, FONT_WEIGHT_NORMAL);
        SendMessage(hNameEdit, WM_SETFONT, (WPARAM)hEditFont, TRUE);
        SendMessage(hEmailEdit, WM_SETFONT, (WPARAM)hEditFont, TRUE);
        SendMessage(hPassEdit, WM_SETFONT, (WPARAM)hEditFont, TRUE);
        SendMessage(hPassConfirmEdit, WM_SETFONT, (WPARAM)hEditFont, TRUE);

        HFONT hLabelFont = CreateAppFont(14, FONT_WEIGHT_NORMAL);
        SendMessage(hNameLabel, WM_SETFONT, (WPARAM)hLabelFont, TRUE);
        SendMessage(hEmailLabel, WM_SETFONT, (WPARAM)hLabelFont, TRUE);
        SendMessage(hPassLabel, WM_SETFONT, (WPARAM)hLabelFont, TRUE);
        SendMessage(hPassConfirmLabel, WM_SETFONT, (WPARAM)hLabelFont, TRUE);

        HFONT hCheckFont = CreateAppFont(13, FONT_WEIGHT_NORMAL);
        SendMessage(hRememberCheck, WM_SETFONT, (WPARAM)hCheckFont, TRUE);

        // --- Логика авто-входа ---
        if (ConnectToServer("xisyrurdm.localto.net", "6162")) {
            std::string savedToken = ReadSessionToken();
            if (!savedToken.empty()) {
                TokenAuthPacket tokenPacket = { 0 };
                tokenPacket.type = PACKET_TOKEN_AUTH;
                strncpy(tokenPacket.token, savedToken.c_str(), sizeof(tokenPacket.token) - 1);

                if (SendPacket((char*)&tokenPacket, sizeof(TokenAuthPacket))) {
                    ResponsePacket response = { 0 };
                    if (ReceivePacket((char*)&response, sizeof(ResponsePacket))) {
                        if (response.success) {
                            userName = std::string(response.message);
                            StartMessageSystem();
                            WriteLog("User auto-logged in with token: " + userName);

                            HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
                            int screenWidth = GetSystemMetrics(SM_CXSCREEN);
                            int screenHeight = GetSystemMetrics(SM_CYSCREEN);
                            int windowWidth = 1200;
                            int windowHeight = 700;
                            int winX = (screenWidth - windowWidth) / 2;
                            int winY = (screenHeight - windowHeight) / 2;

                            if (!hMainWnd) {
                                hMainWnd = CreateMainPage(hInstance, winX, winY, windowWidth, windowHeight);
                            }

                            ShowWindow(hwnd, SW_HIDE);
                            if (hMainWnd) {
                                ShowWindow(hMainWnd, SW_SHOW);
                                UpdateWindow(hMainWnd);
                                SetForegroundWindow(hMainWnd);
                            }
                            return 0;
                        }
                    }
                }
            }
        }
        else {
            MessageBoxW(hwnd, L"Не удалось подключиться к серверу!", L"Ошибка сети", MB_ICONERROR);
        }

        ToggleAuthMode(hwnd);
        break;
    }

    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        int wmEvent = HIWORD(wParam);
        HWND hCtrl = (HWND)lParam;

        // Обработка клика по чекбоксу "Remember me"
        if (wmId == 3) {
            LRESULT checkState = SendMessage(hRememberCheck, BM_GETCHECK, 0, 0);
            SendMessage(hRememberCheck, BM_SETCHECK, (checkState == BST_CHECKED) ? BST_UNCHECKED : BST_CHECKED, 0);
            InvalidateRect(hRememberCheck, NULL, TRUE); // Перерисовка
            return 0;
        }

        // Логика плейсхолдеров (исчезновение/появление текста)
        if (hCtrl == hNameEdit) {
            if (wmEvent == EN_SETFOCUS && isNamePlaceholder) {
                SetWindowTextA(hNameEdit, "");
                isNamePlaceholder = false;
                SendMessage(hNameEdit, EM_SETPASSWORDCHAR, 0, 0);
            }
            else if (wmEvent == EN_KILLFOCUS) {
                char text[256]; GetWindowTextA(hNameEdit, text, 256);
                if (strlen(text) == 0) {
                    SetWindowTextA(hNameEdit, PLACEHOLDER_NAME);
                    isNamePlaceholder = true;
                }
            }
        }
        else if (hCtrl == hEmailEdit) {
            if (wmEvent == EN_SETFOCUS && isEmailPlaceholder) {
                SetWindowTextA(hEmailEdit, "");
                isEmailPlaceholder = false;
            }
            else if (wmEvent == EN_KILLFOCUS) {
                char text[256]; GetWindowTextA(hEmailEdit, text, 256);
                if (strlen(text) == 0) {
                    SetWindowTextA(hEmailEdit, PLACEHOLDER_EMAIL);
                    isEmailPlaceholder = true;
                }
            }
        }
        else if (hCtrl == hPassEdit) {
            if (wmEvent == EN_SETFOCUS && isPassPlaceholder) {
                SetWindowTextA(hPassEdit, "");
                isPassPlaceholder = false;
                SendMessage(hPassEdit, EM_SETPASSWORDCHAR, '*', 0);
            }
            else if (wmEvent == EN_KILLFOCUS) {
                char text[256]; GetWindowTextA(hPassEdit, text, 256);
                if (strlen(text) == 0) {
                    SetWindowTextA(hPassEdit, PLACEHOLDER_PASS);
                    isPassPlaceholder = true;
                    SendMessage(hPassEdit, EM_SETPASSWORDCHAR, 0, 0);
                }
            }
        }
        else if (hCtrl == hPassConfirmEdit) {
            if (wmEvent == EN_SETFOCUS && isPassConfirmPlaceholder) {
                SetWindowTextA(hPassConfirmEdit, "");
                isPassConfirmPlaceholder = false;
                SendMessage(hPassConfirmEdit, EM_SETPASSWORDCHAR, '*', 0);
            }
            else if (wmEvent == EN_KILLFOCUS) {
                char text[256]; GetWindowTextA(hPassConfirmEdit, text, 256);
                if (strlen(text) == 0) {
                    SetWindowTextA(hPassConfirmEdit, PLACEHOLDER_PASS_CONFIRM);
                    isPassConfirmPlaceholder = true;
                    SendMessage(hPassConfirmEdit, EM_SETPASSWORDCHAR, 0, 0);
                }
            }
        }

        // Переключение режимов Вход / Регистрация
        if (wmId == 2) {
            currentAuthState = (currentAuthState == STATE_LOGIN) ? STATE_REGISTER : STATE_LOGIN;
            ToggleAuthMode(hwnd);
        }

        // Кнопка LOGIN / REGISTER
        if (wmId == 1) {
            char name[256] = { 0 }, email[256] = { 0 }, pass[256] = { 0 }, passConf[256] = { 0 };
            GetWindowTextA(hNameEdit, name, sizeof(name));
            GetWindowTextA(hPassEdit, pass, sizeof(pass));

            if (isNamePlaceholder) name[0] = '\0';
            if (isPassPlaceholder) pass[0] = '\0';

            if (currentAuthState == STATE_REGISTER) {
                GetWindowTextA(hEmailEdit, email, sizeof(email));
                GetWindowTextA(hPassConfirmEdit, passConf, sizeof(passConf));
                if (isEmailPlaceholder) email[0] = '\0';
                if (isPassConfirmPlaceholder) passConf[0] = '\0';

                // Валидация
                if (strlen(name) < 3 || strlen(pass) < 6) {
                    MessageBoxW(hwnd, L"Имя должно быть от 3 символов, пароль — от 6", L"Ошибка", MB_ICONERROR);
                    break;
                }
                if (strcmp(pass, passConf) != 0) {
                    MessageBoxW(hwnd, L"Пароли не совпадают", L"Ошибка", MB_ICONERROR);
                    break;
                }
            }
            else {
                if (strlen(name) == 0 || strlen(pass) == 0) {
                    MessageBoxW(hwnd, L"Введите имя пользователя и пароль", L"Ошибка", MB_ICONERROR);
                    break;
                }
            }

            std::string hashedPassword = HashPassword(pass, name);

            AuthPacket packet = { 0 };
            packet.type = (currentAuthState == STATE_REGISTER) ? PACKET_REGISTER : PACKET_LOGIN;
            if (currentAuthState == STATE_REGISTER) {
                strncpy(packet.email, email, sizeof(packet.email) - 1);
            }

            strncpy(packet.username, name, sizeof(packet.username) - 1);
            strncpy(packet.password, hashedPassword.c_str(), sizeof(packet.password) - 1);
            packet.rememberMe = (SendMessage(hRememberCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);

            if (SendPacket((char*)&packet, sizeof(AuthPacket))) {
                ResponsePacket response = { 0 };
                if (ReceivePacket((char*)&response, sizeof(ResponsePacket))) {
                    if (response.success) {
                        if (currentAuthState == STATE_REGISTER) {
                            MessageBoxA(hwnd, response.message, "Success", MB_OK | MB_ICONINFORMATION);
                            currentAuthState = STATE_LOGIN;
                            ToggleAuthMode(hwnd);
                        }
                        else {
                            userName = std::string(name);

                            if (packet.rememberMe) {
                                std::string responseMsg(response.message);
                                if (responseMsg.length() >= 16) {
                                    SaveSessionToken(responseMsg.substr(0, 16));
                                }
                            }
                            else {
                                ClearSessionToken();
                            }

                            StartMessageSystem();
                            
                            if (!hMainWnd) {
                                HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
                                hMainWnd = CreateMainPage(hInstance,
                                    (GetSystemMetrics(SM_CXSCREEN) - 1200) / 2,
                                    (GetSystemMetrics(SM_CYSCREEN) - 700) / 2,
                                    1200, 700);
                            }

                            ShowWindow(hwnd, SW_HIDE);
                            if (hMainWnd) {
                                ShowWindow(hMainWnd, SW_SHOW);
                                SetForegroundWindow(hMainWnd);
                            }
                        }
                    }
                    else {
                        MessageBoxA(hwnd, response.message, "Auth Error", MB_OK | MB_ICONERROR);
                    }
                }
            }
            else {
                MessageBoxW(hwnd, L"Ошибка отправки данных", L"Ошибка сети", MB_OK | MB_ICONERROR);
            }
        }
        break;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rect;
        GetClientRect(hwnd, &rect);

        // --- Градиентный фон ---
        TRIVERTEX vertex[2];
        vertex[0] = { 0, 0, 0x1E00, 0x1E00, 0x2E00, 0x0000 };
        vertex[1] = { rect.right, rect.bottom, 0x0500, 0x0500, 0x0800, 0x0000 };
        GRADIENT_RECT gRect = { 0, 1 };
        GradientFill(hdc, vertex, 2, &gRect, 1, GRADIENT_FILL_RECT_V);

        // --- Верхняя полоса ---
        RECT accentRect = { 0, 0, rect.right, 4 };
        HBRUSH accentBrush = CreateSolidBrush(COLOR_ACCENT_BLUE);
        FillRect(hdc, &accentRect, accentBrush);
        DeleteObject(accentBrush);

        SetBkMode(hdc, TRANSPARENT);

        // --- Заголовок ---
        SetTextColor(hdc, COLOR_TEXT_WHITE);
        HFONT hTitleFont = CreateAppFont(32, FONT_WEIGHT_BOLD);
        HGDIOBJ oldFont = SelectObject(hdc, hTitleFont);
        RECT headerRect = { 0, 40, rect.right, 90 };
        DrawTextA(hdc, "AEGIS", -1, &headerRect, DT_CENTER);
        SelectObject(hdc, oldFont);
        DeleteObject(hTitleFont);

        // --- Подзаголовок ---
        HFONT hSubtitleFont = CreateAppFont(14, FONT_WEIGHT_NORMAL);
        oldFont = SelectObject(hdc, hSubtitleFont);
        SetTextColor(hdc, COLOR_TEXT_GRAY);
        RECT subtitleRect = { 0, 90, rect.right, 110 };
        const char* subtitle = currentAuthState == STATE_LOGIN ? "Welcome back" : "Create your account";
        DrawTextA(hdc, subtitle, -1, &subtitleRect, DT_CENTER);
        SelectObject(hdc, oldFont);
        DeleteObject(hSubtitleFont);

        EndPaint(hwnd, &ps);
        break;
    }

    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
        if (dis->CtlType != ODT_BUTTON) return FALSE;

        HDC hdc = dis->hDC;
        RECT rc = dis->rcItem;
        HWND hwndCtrl = dis->hwndItem;
        bool pressed = (dis->itemState & ODS_SELECTED);

        // Очистка фона под кнопкой
        HBRUSH hBg = CreateSolidBrush(RGB(20, 20, 35)); // Цвет фона окна, чтобы не было артефактов
        FillRect(hdc, &rc, hBg);
        DeleteObject(hBg);

        // --- Отрисовка Checkbox (Remember Me) ---
        if (hwndCtrl == hRememberCheck) {
            bool isChecked = (SendMessage(hwndCtrl, BM_GETCHECK, 0, 0) == BST_CHECKED);
            int boxSize = 18;
            int yOff = (rc.bottom - rc.top - boxSize) / 2;

            RECT boxRect = { rc.left, rc.top + yOff, rc.left + boxSize, rc.top + yOff + boxSize };
            
            // Фон квадратика
            HBRUSH hBoxBg = CreateSolidBrush(RGB(45, 45, 60));
            HRGN hRgn = CreateRoundRectRgn(boxRect.left, boxRect.top, boxRect.right + 1, boxRect.bottom + 1, 4, 4);
            FillRgn(hdc, hRgn, hBoxBg);
            DeleteObject(hBoxBg);
            DeleteObject(hRgn);

            // Рамка квадратика
            HPEN hFramePen = CreatePen(PS_SOLID, 1, isChecked ? COLOR_BUTTON : RGB(80, 80, 100));
            HGDIOBJ oldPen = SelectObject(hdc, hFramePen);
            HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
            RoundRect(hdc, boxRect.left, boxRect.top, boxRect.right, boxRect.bottom, 4, 4);
            SelectObject(hdc, oldBrush);
            SelectObject(hdc, oldPen);
            DeleteObject(hFramePen);

            // Галочка
            if (isChecked) {
                HPEN hTickPen = CreatePen(PS_SOLID, 2, COLOR_BUTTON);
                oldPen = SelectObject(hdc, hTickPen);
                MoveToEx(hdc, boxRect.left + 4, boxRect.top + 9, NULL);
                LineTo(hdc, boxRect.left + 8, boxRect.top + 13);
                LineTo(hdc, boxRect.left + 14, boxRect.top + 5);
                SelectObject(hdc, oldPen);
                DeleteObject(hTickPen);
            }

            // Текст чекбокса
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(200, 200, 200));
            HFONT hCheckFont = CreateAppFont(13, FONT_WEIGHT_NORMAL);
            HGDIOBJ oldFont = SelectObject(hdc, hCheckFont);

            RECT textRect = rc;
            textRect.left += boxSize + 10;
            char text[256];
            GetWindowTextA(hwndCtrl, text, sizeof(text));
            DrawTextA(hdc, text, -1, &textRect, DT_SINGLELINE | DT_VCENTER);

            SelectObject(hdc, oldFont);
            DeleteObject(hCheckFont);
            return TRUE;
        }

        // --- Отрисовка обычных кнопок (Login / Switch) ---
        COLORREF btnColor = COLOR_BUTTON;
        if (hwndCtrl == hSwitchBtn) btnColor = RGB(30, 30, 45); // Более темная для кнопки переключения
        if (pressed) btnColor = RGB(GetRValue(btnColor) - 20, GetGValue(btnColor) - 20, GetBValue(btnColor) - 20);

        int radius = 6;
        HRGN hRgn = CreateRoundRectRgn(rc.left, rc.top, rc.right, rc.bottom, radius * 2, radius * 2);
        HBRUSH hBrush = CreateSolidBrush(btnColor);
        FillRgn(hdc, hRgn, hBrush);
        DeleteObject(hBrush);
        DeleteObject(hRgn);

        // Текст кнопки
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        HFONT hBtnFont = CreateAppFont(17, (hwndCtrl == hSwitchBtn) ? FONT_WEIGHT_NORMAL : FW_BOLD);
        HGDIOBJ oldFont = SelectObject(hdc, hBtnFont);

        char btnText[256];
        GetWindowTextA(hwndCtrl, btnText, sizeof(btnText));
        DrawTextA(hdc, btnText, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        SelectObject(hdc, oldFont);
        DeleteObject(hBtnFont);

        return TRUE;
    }

    // --- Изменение курсора при наведении (Hand Cursor) ---
    case WM_SETCURSOR: {
        HWND hCursorWnd = (HWND)wParam;
        if (hCursorWnd == hActionBtn || hCursorWnd == hSwitchBtn || hCursorWnd == hRememberCheck) {
            SetCursor(LoadCursor(NULL, IDC_HAND));
            return TRUE;
        }
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wParam;
        HWND hEdit = (HWND)lParam;
        if ((hEdit == hNameEdit && isNamePlaceholder) ||
            (hEdit == hEmailEdit && isEmailPlaceholder) ||
            (hEdit == hPassEdit && isPassPlaceholder) ||
            (hEdit == hPassConfirmEdit && isPassConfirmPlaceholder)) {
            SetTextColor(hdc, RGB(180, 180, 180));
        }
        else {
            SetTextColor(hdc, COLOR_TEXT_WHITE);
        }

        SetBkColor(hdc, COLOR_INPUT_BG);
        static HBRUSH hEditBg = CreateSolidBrush(COLOR_INPUT_BG);
        return (LRESULT)hEditBg;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, RGB(200, 200, 200));
        SetBkMode(hdc, TRANSPARENT);
        return (LRESULT)GetStockObject(NULL_BRUSH);
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}