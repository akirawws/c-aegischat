#include "MainPage.h"
#include "Utils/Styles.h"
#include "Components/Sidebar.h"
#include "Components/MessageList.h"
#include "Components/MessageInput.h"
#include "Utils/Keyboard.h"
#include "Utils/Network.h"
#include <vector>

HWND hMainWnd = NULL;

extern int inputEditHeight;
extern HWND hMessageList;
extern HWND hInputEdit;
extern HWND hSidebar;

HWND CreateMainPage(HINSTANCE hInstance, int x, int y, int width, int height) {
    hMainWnd = CreateWindowExA(0, "MainWindow", "AEGIS - Chat",
        WS_OVERLAPPEDWINDOW,
        x, y, width, height,
        NULL, NULL, hInstance, NULL);
    return hMainWnd;
}

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        CreateSidebar(hwnd, 0, 0, 250, 600);
        CreateMessageList(hwnd, 250, 0, 750, 550);
        CreateMessageInput(hwnd, 260, 550, 740, INPUT_MIN_HEIGHT);
        break;
    }
    
    case WM_COMMAND:
        if (HIWORD(wParam) == EN_CHANGE && (HWND)lParam == hInputEdit) {
            InvalidateRect(hwnd, NULL, FALSE); 
            UpdateInputHeight(hwnd, hInputEdit, hMessageList);
        }
        break;

    case WM_CTLCOLOREDIT: {
        if ((HWND)lParam == hInputEdit) {
            HDC hdc = (HDC)wParam;
            SetBkMode(hdc, OPAQUE); 
            SetBkColor(hdc, COLOR_INPUT_BG);
            SetTextColor(hdc, COLOR_TEXT_WHITE);
            
            static HBRUSH hInputBgBrush = CreateSolidBrush(COLOR_INPUT_BG);
            return (LRESULT)hInputBgBrush;
        }
        break;
    }
    
    case WM_KEYDOWN:
    case WM_CHAR:
        if (HandleKeyboardInput(hwnd, uMsg, wParam, lParam, hInputEdit)) {
            return 0;
        }
        break;
        
    case WM_SETFOCUS:
        if ((HWND)wParam == hInputEdit || GetFocus() == hInputEdit) {
            InvalidateRect(hwnd, NULL, TRUE);
        }
        break;
        
    case WM_KILLFOCUS:
        if ((HWND)wParam == hInputEdit || GetFocus() != hInputEdit) {
            InvalidateRect(hwnd, NULL, TRUE);
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
        
        DrawInputField(hdc, hwnd, hInputEdit);
        
        EndPaint(hwnd, &ps);
        break;
    }
    
    case WM_SIZE: {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);
        MoveWindow(hSidebar, 0, 0, 250, height, TRUE);
        MoveWindow(hMessageList, 250, 0, width - 250, height - inputEditHeight - 20, TRUE);
        MoveWindow(hInputEdit, 260, height - inputEditHeight - 10, width - 270, inputEditHeight, TRUE);
        InvalidateRect(hwnd, NULL, TRUE);
        InvalidateRect(hSidebar, NULL, TRUE);
        InvalidateRect(hMessageList, NULL, TRUE);
        break;
    }
    
    case WM_DESTROY:
        isConnected = false;
        if (clientSocket != INVALID_SOCKET) {
            closesocket(clientSocket);
        }
        if (receiveThread.joinable()) {
            receiveThread.join();
        }
        WSACleanup();
        PostQuitMessage(0);
        break;
        
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

