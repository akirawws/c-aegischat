#include "AuthPage.h"
#include "Utils/Styles.h"
#include "Utils/Utils.h"
#include "Utils/Network.h"
#include "Utils/AuthProtocol.h"
#include "Utils/HashPassword.h"
#include <cstring>
#include <string>

// Состояния страницы
enum AuthState { STATE_LOGIN, STATE_REGISTER };
AuthState currentAuthState = STATE_LOGIN;

// Хендлы элементов управления
HWND hAuthWnd = NULL;
HWND hNameEdit = NULL;     
HWND hEmailEdit = NULL;    
HWND hPassEdit = NULL;
HWND hPassConfirmEdit = NULL;
HWND hRememberCheck = NULL;
HWND hActionBtn = NULL;    
HWND hSwitchBtn = NULL;    

extern HWND hMainWnd;
extern std::string userName; // Глобальная переменная для хранения имени вошедшего пользователя

// Вспомогательная функция для переключения видимости
void ToggleAuthMode(HWND hwnd) {
    if (currentAuthState == STATE_LOGIN) {
        ShowWindow(hEmailEdit, SW_HIDE);
        ShowWindow(hPassConfirmEdit, SW_HIDE);
        ShowWindow(hRememberCheck, SW_SHOW);
        SetWindowTextA(hActionBtn, "Login");
        SetWindowTextA(hSwitchBtn, "Don't have an account? Register");
    } else {
        ShowWindow(hEmailEdit, SW_SHOW);
        ShowWindow(hPassConfirmEdit, SW_SHOW);
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

    // Увеличим высоту до 520
    hAuthWnd = CreateWindowA(
        "AuthWindow",
        "AEGIS — Authorization",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        x, y, width, 520,   
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
        int fieldW = 300;
        int fieldH = 35;
        int startX = (420 - fieldW) / 2 - 10;

        hNameEdit = CreateWindowA("EDIT", "", WS_VISIBLE | WS_CHILD | ES_LEFT | WS_BORDER | ES_AUTOHSCROLL,
            startX, 120, fieldW, fieldH, hwnd, NULL, NULL, NULL);

        hEmailEdit = CreateWindowA("EDIT", "", WS_CHILD | ES_LEFT | WS_BORDER | ES_AUTOHSCROLL,
            startX, 185, fieldW, fieldH, hwnd, NULL, NULL, NULL);

        hPassEdit = CreateWindowA("EDIT", "", WS_VISIBLE | WS_CHILD | ES_LEFT | WS_BORDER | ES_PASSWORD,
            startX, 250, fieldW, fieldH, hwnd, NULL, NULL, NULL);

        hPassConfirmEdit = CreateWindowA("EDIT", "", WS_CHILD | ES_LEFT | WS_BORDER | ES_PASSWORD,
            startX, 315, fieldW, fieldH, hwnd, NULL, NULL, NULL);

        hRememberCheck = CreateWindowA("BUTTON", "Remember me on this device", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
            startX, 290, fieldW, 20, hwnd, NULL, NULL, NULL);

        hActionBtn = CreateWindowA("BUTTON", "Login", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
            startX, 360, fieldW, 45, hwnd, (HMENU)1, NULL, NULL);

        hSwitchBtn = CreateWindowA("BUTTON", "Register", WS_VISIBLE | WS_CHILD | BS_FLAT,
            startX, 415, fieldW, 25, hwnd, (HMENU)2, NULL, NULL);

        HFONT hEditFont = CreateAppFont(18, FONT_WEIGHT_NORMAL);
        SendMessage(hNameEdit, WM_SETFONT, (WPARAM)hEditFont, TRUE);
        SendMessage(hEmailEdit, WM_SETFONT, (WPARAM)hEditFont, TRUE);
        SendMessage(hPassEdit, WM_SETFONT, (WPARAM)hEditFont, TRUE);
        SendMessage(hPassConfirmEdit, WM_SETFONT, (WPARAM)hEditFont, TRUE);
        
        if (!ConnectToServer("xisyrurdm.localto.net", "6162")) {
            MessageBoxW(hwnd, L"Не удалось подключиться к серверу!", L"Ошибка сети", MB_ICONERROR);
        }

        ToggleAuthMode(hwnd);
        break;
    }

    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        
        if (wmId == 2) {
            currentAuthState = (currentAuthState == STATE_LOGIN) ? STATE_REGISTER : STATE_LOGIN;
            ToggleAuthMode(hwnd);
        }

        if (wmId == 1) { 
            char name[256], email[256], pass[256], passConf[256];
            GetWindowTextA(hNameEdit, name, 256);
            GetWindowTextA(hPassEdit, pass, 256);

            // 1. Проверки для регистрации
            if (currentAuthState == STATE_REGISTER) {
                GetWindowTextA(hEmailEdit, email, 256);
                GetWindowTextA(hPassConfirmEdit, passConf, 256);

                if (strlen(name) < 3 || strlen(pass) < 6) {
                    MessageBoxW(hwnd, L"Имя > 3, Пароль > 6 символов", L"Ошибка", MB_ICONERROR);
                    break;
                }
                if (strcmp(pass, passConf) != 0) {
                    MessageBoxW(hwnd, L"Пароли не совпадают", L"Ошибка", MB_ICONERROR);
                    break;
                }
            }

            // 2. ХЕШИРОВАНИЕ ПАРОЛЯ
            // Используем имя пользователя (name) в качестве соли. 
            // Это гарантирует, что даже одинаковые пароли у разных юзеров будут иметь разные хеши.
            std::string hashedPassword = HashPassword(pass, name);

            // 3. Подготовка пакета
            AuthPacket packet = { 0 };
            if (currentAuthState == STATE_REGISTER) {
                packet.type = PACKET_REGISTER;
                strncpy(packet.email, email, sizeof(packet.email) - 1);
            } else {
                packet.type = PACKET_LOGIN;
            }

            strncpy(packet.username, name, sizeof(packet.username) - 1);
            
            // ВАЖНО: Копируем именно hashedPassword вместо чистого pass
            strncpy(packet.password, hashedPassword.c_str(), sizeof(packet.password) - 1); 
            
            packet.rememberMe = (SendMessage(hRememberCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);

            // 4. Отправка и обработка ответа
            if (SendPacket((char*)&packet, sizeof(AuthPacket))) {
                ResponsePacket response = { 0 };
                if (ReceivePacket((char*)&response, sizeof(ResponsePacket))) {
                    if (response.success) {
                        if (currentAuthState == STATE_REGISTER) {
                            MessageBoxA(hwnd, response.message, "Success", MB_OK | MB_ICONINFORMATION);
                            currentAuthState = STATE_LOGIN;
                            ToggleAuthMode(hwnd);
                        } else {
                            userName = name; 
                            StartMessageSystem(); // Запуск фонового потока чата
                            WriteLog("User logged in: " + userName);
                            ShowWindow(hwnd, SW_HIDE);
                            ShowWindow(hMainWnd, SW_SHOW);
                        }
                    } else {
                        MessageBoxA(hwnd, response.message, "Auth Error", MB_OK | MB_ICONERROR);
                    }
                } else {
                    MessageBoxW(hwnd, L"Сервер не отвечает", L"Ошибка", MB_OK | MB_ICONERROR);
                }
            } else {
                MessageBoxW(hwnd, L"Ошибка отправки данных", L"Ошибка сети", MB_OK | MB_ICONERROR);
            }
        }
        break;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rect; GetClientRect(hwnd, &rect);
        
        HBRUSH brush = CreateSolidBrush(COLOR_BG_DARK);
        FillRect(hdc, &rect, brush);
        DeleteObject(brush);

        SetTextColor(hdc, COLOR_TEXT_WHITE);
        SetBkMode(hdc, TRANSPARENT);
        
        HFONT hTitleFont = CreateAppFont(28, FONT_WEIGHT_BOLD);
        SelectObject(hdc, hTitleFont);
        RECT headerRect = {0, 30, rect.right, 75};
        DrawTextA(hdc, "AEGIS SYSTEM", -1, &headerRect, DT_CENTER);
        DeleteObject(hTitleFont);

        HFONT hLabelFont = CreateAppFont(14, FONT_WEIGHT_NORMAL);
        SelectObject(hdc, hLabelFont);
        SetTextColor(hdc, COLOR_TEXT_GRAY);

        if (currentAuthState == STATE_LOGIN) {
            TextOutA(hdc, 55, 100, "Username or Email", 17);
            TextOutA(hdc, 55, 230, "Password", 8);
        } else {
            TextOutA(hdc, 55, 100, "Desired Nametag", 15);
            TextOutA(hdc, 55, 165, "Email Address", 13);
            TextOutA(hdc, 55, 230, "Password", 8);
            TextOutA(hdc, 55, 295, "Confirm Password", 16);
        }

        DeleteObject(hLabelFont);
        EndPaint(hwnd, &ps);
        break;
    }

    case WM_DRAWITEM: {
        if (wParam == 1) { 
            LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
            HBRUSH hBtnBrush = CreateSolidBrush((dis->itemState & ODS_SELECTED) ? COLOR_ACCENT_BLUE_DARK : COLOR_ACCENT_BLUE);
            FillRect(dis->hDC, &dis->rcItem, hBtnBrush);
            DeleteObject(hBtnBrush);
            SetTextColor(dis->hDC, COLOR_TEXT_WHITE);
            SetBkMode(dis->hDC, TRANSPARENT);
            HFONT hBtnFont = CreateAppFont(16, FONT_WEIGHT_BOLD);
            SelectObject(dis->hDC, hBtnFont);
            char btnText[32]; GetWindowTextA(dis->hwndItem, btnText, 32);
            DrawTextA(dis->hDC, btnText, -1, &dis->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            DeleteObject(hBtnFont);
            return TRUE;
        }
        break;
    }

    case WM_CTLCOLOREDIT: 
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, COLOR_TEXT_WHITE);
        SetBkColor(hdc, COLOR_BG_DARK);
        static HBRUSH hBg = CreateSolidBrush(COLOR_BG_DARK);
        return (LRESULT)hBg;
    }

    case WM_DESTROY: PostQuitMessage(0); break;
    default: return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}