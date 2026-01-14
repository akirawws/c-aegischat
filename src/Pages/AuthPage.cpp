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

extern HWND hMainWnd;

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

        hNameEdit = CreateWindowA("EDIT", "", WS_VISIBLE | WS_CHILD | ES_LEFT | WS_BORDER | ES_AUTOHSCROLL,
            startX, 130, fieldW, fieldH, hwnd, NULL, NULL, NULL);

        hEmailEdit = CreateWindowA("EDIT", "", WS_CHILD | ES_LEFT | WS_BORDER | ES_AUTOHSCROLL,
            startX, 200, fieldW, fieldH, hwnd, NULL, NULL, NULL);

        hPassEdit = CreateWindowA("EDIT", "", WS_VISIBLE | WS_CHILD | ES_LEFT | WS_BORDER | ES_PASSWORD,
            startX, 270, fieldW, fieldH, hwnd, NULL, NULL, NULL);

        hPassConfirmEdit = CreateWindowA("EDIT", "", WS_CHILD | ES_LEFT | WS_BORDER | ES_PASSWORD,
            startX, 340, fieldW, fieldH, hwnd, NULL, NULL, NULL);

        hRememberCheck = CreateWindowA("BUTTON", "Remember me on this device", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
            startX, 320, fieldW, 20, hwnd, NULL, NULL, NULL);

        hActionBtn = CreateWindowA("BUTTON", "Login", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            startX, 380, fieldW, 48, hwnd, (HMENU)1, NULL, NULL);

        hSwitchBtn = CreateWindowA("BUTTON", "Don't have an account? Register", WS_VISIBLE | WS_CHILD | BS_FLAT,
            startX, 440, fieldW, 30, hwnd, (HMENU)2, NULL, NULL);

        HFONT hEditFont = CreateAppFont(16, FONT_WEIGHT_NORMAL);
        SendMessage(hNameEdit, WM_SETFONT, (WPARAM)hEditFont, TRUE);
        SendMessage(hEmailEdit, WM_SETFONT, (WPARAM)hEditFont, TRUE);
        SendMessage(hPassEdit, WM_SETFONT, (WPARAM)hEditFont, TRUE);
        SendMessage(hPassConfirmEdit, WM_SETFONT, (WPARAM)hEditFont, TRUE);
        
        HFONT hCheckFont = CreateAppFont(13, FONT_WEIGHT_NORMAL);
        SendMessage(hRememberCheck, WM_SETFONT, (WPARAM)hCheckFont, TRUE);
        
        // Проверяем токен при подключении
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
                            // Username приходит в response.message
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
        } else {
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

            std::string hashedPassword = HashPassword(pass, name);

            AuthPacket packet = { 0 };
            if (currentAuthState == STATE_REGISTER) {
                packet.type = PACKET_REGISTER;
                strncpy(packet.email, email, sizeof(packet.email) - 1);
            } else {
                packet.type = PACKET_LOGIN;
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
                        } else {
                            userName = name; 
                            
                            // Сохраняем токен, если rememberMe = true
                            // Токен приходит в response.message (первые 16 символов)
                            if (packet.rememberMe) {
                                std::string responseMsg(response.message);
                                if (responseMsg.length() >= 16) {
                                    std::string token = responseMsg.substr(0, 16);
                                    SaveSessionToken(token);
                                    WriteLog("Session token saved for user: " + userName);
                                }
                            } else {
                                // Очищаем токен, если пользователь не хочет запоминать
                                ClearSessionToken();
                            }
                            
                            StartMessageSystem();
                            WriteLog("User logged in: " + userName);
                            
                            if (!hMainWnd) {
                                WriteLog("Creating main window...");
                                
                                HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
                                

                                int screenWidth = GetSystemMetrics(SM_CXSCREEN);
                                int screenHeight = GetSystemMetrics(SM_CYSCREEN);
                                int windowWidth = 1200;
                                int windowHeight = 700;
                                int x = (screenWidth - windowWidth) / 2;
                                int y = (screenHeight - windowHeight) / 2;
                                
                                hMainWnd = CreateMainPage(hInstance, x, y, windowWidth, windowHeight);
                                
                                if (!hMainWnd) {
                                    WriteLog("Failed to create main window! Error: " + std::to_string(GetLastError()));
                                    MessageBoxA(hwnd, "Failed to create main window", "Error", MB_OK | MB_ICONERROR);
                                    return 0;
                                }
                                
                                WriteLog("Main window created successfully. Handle: " + 
                                        std::to_string((long long)hMainWnd));
                            }
                            
                            ShowWindow(hwnd, SW_HIDE);

                            if (hMainWnd) {
                                WriteLog("Showing main window...");
                                ShowWindow(hMainWnd, SW_SHOW);
                                UpdateWindow(hMainWnd);
                                SetForegroundWindow(hMainWnd);
                                SetFocus(hMainWnd);
                                SetActiveWindow(hMainWnd);
                                BringWindowToTop(hMainWnd);
                            }
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
        RECT rect;
        GetClientRect(hwnd, &rect);
    
        // --- Градиентный фон ---
        TRIVERTEX vertex[2];
        vertex[0].x = 0;
        vertex[0].y = 0;
        vertex[0].Red = 0x1E00;   // тёмно-синий верх
        vertex[0].Green = 0x1E00;
        vertex[0].Blue = 0x2E00;
        vertex[0].Alpha = 0x0000;
    
        vertex[1].x = rect.right;
        vertex[1].y = rect.bottom;
        vertex[1].Red = 0x0500;   // чуть светлее низ
        vertex[1].Green = 0x0500;
        vertex[1].Blue = 0x0800;
        vertex[1].Alpha = 0x0000;
    
        GRADIENT_RECT gRect = { 0, 1 };
        GradientFill(hdc, vertex, 2, &gRect, 1, GRADIENT_FILL_RECT_V); // вертикальный плавный градиент
        // ------------------------
    
        // Верхняя полоса с акцентом
        RECT accentRect = {0, 0, rect.right, 4};
        HBRUSH accentBrush = CreateSolidBrush(COLOR_ACCENT_BLUE);
        FillRect(hdc, &accentRect, accentBrush);
        DeleteObject(accentBrush);
    
        SetTextColor(hdc, COLOR_TEXT_WHITE);
        SetBkMode(hdc, TRANSPARENT);
        
        // Остальной код рисования текста, заголовка и меток
        HFONT hTitleFont = CreateAppFont(32, FONT_WEIGHT_BOLD);
        SelectObject(hdc, hTitleFont);
        RECT headerRect = {0, 40, rect.right, 90};
        DrawTextA(hdc, "AEGIS", -1, &headerRect, DT_CENTER);
        DeleteObject(hTitleFont);
    
        HFONT hSubtitleFont = CreateAppFont(14, FONT_WEIGHT_NORMAL);
        SelectObject(hdc, hSubtitleFont);
        SetTextColor(hdc, COLOR_TEXT_GRAY);
        RECT subtitleRect = {0, 90, rect.right, 110};
        const char* subtitle = currentAuthState == STATE_LOGIN ? "Welcome back" : "Create your account";
        DrawTextA(hdc, subtitle, -1, &subtitleRect, DT_CENTER);
        DeleteObject(hSubtitleFont);
    
        EndPaint(hwnd, &ps);
        break;
    }

    case WM_DRAWITEM: {
        if (wParam == 1) { 
            LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
    
            // Цвет фона по состоянию
            COLORREF btnColor;
            if (dis->itemState & ODS_SELECTED) {
                btnColor = RGB(35, 90, 190); // при нажатии
            } else if (dis->itemState & ODS_HOTLIGHT) {
                btnColor = RGB(65, 140, 240); // при наведении
            } else {
                btnColor = RGB(45, 110, 220); // обычное состояние
            }
    
            // Цвет текста
            COLORREF textColor = RGB(255, 255, 255); // белый
    
            // Фон кнопки
            HBRUSH hBtnBrush = CreateSolidBrush(btnColor);
            FillRect(dis->hDC, &dis->rcItem, hBtnBrush);
            DeleteObject(hBtnBrush);
    
            // Рамка (немного светлее фона)
            HPEN hPen = CreatePen(PS_SOLID, 1, RGB(80, 160, 255));
            HPEN hOldPen = (HPEN)SelectObject(dis->hDC, hPen);
            Rectangle(dis->hDC, dis->rcItem.left, dis->rcItem.top, dis->rcItem.right, dis->rcItem.bottom);
            SelectObject(dis->hDC, hOldPen);
            DeleteObject(hPen);
    
            // Настройка текста
            SetTextColor(dis->hDC, textColor);
            SetBkMode(dis->hDC, TRANSPARENT);
            HFONT hBtnFont = CreateAppFont(17, FONT_WEIGHT_BOLD);
            SelectObject(dis->hDC, hBtnFont);
    
            char btnText[32];
            GetWindowTextA(dis->hwndItem, btnText, 32);
            DrawTextA(dis->hDC, btnText, -1, &dis->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            DeleteObject(hBtnFont);
    
            return TRUE;
        }
        break;
    }
    

    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, COLOR_TEXT_WHITE);
        SetBkColor(hdc, COLOR_INPUT_BG);
        static HBRUSH hEditBg = CreateSolidBrush(COLOR_INPUT_BG);
        return (LRESULT)hEditBg;
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, COLOR_TEXT_WHITE);
        SetBkColor(hdc, COLOR_BG_DARK);
        static HBRUSH hBg = CreateSolidBrush(COLOR_BG_DARK);
        return (LRESULT)hBg;
    }
    case WM_CTLCOLORBTN: {
        HDC hdc = (HDC)wParam;
        if ((HWND)lParam == hActionBtn) {
            SetTextColor(hdc, RGB(255,255,255));        // белый текст
            SetBkColor(hdc, RGB(45,110,220));          // синий фон
            static HBRUSH hBrush = CreateSolidBrush(RGB(45,110,220));
            return (LRESULT)hBrush;
        }
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    

    case WM_DESTROY: PostQuitMessage(0); break;
    default: return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}