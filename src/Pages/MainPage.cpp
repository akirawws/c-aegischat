#include "MainPage.h"
#include "Utils/Styles.h"
#include "Components/Sidebar.h"
#include "Components/MessageList.h"
#include "Components/MessageInput.h"
#include "Pages/FriendsPage.h" // Новый файл для отрисовки меню друзей
#include "Utils/Keyboard.h"
#include "Utils/Network.h"
#include <vector>

HWND hMainWnd = NULL;
extern int inputEditHeight;
extern HWND hMessageList;
extern HWND hInputEdit;
extern HWND hSidebar;

const int DISCORD_SIDEBAR_WIDTH = 72;

// Перечисление состояний приложения
enum class AppState { Friends, Message };
AppState currentState = AppState::Friends; // По умолчанию открываем список друзей

HWND CreateMainPage(HINSTANCE hInstance, int x, int y, int width, int height) {
    hMainWnd = CreateWindowExA(0, "MainWindow", "AEGIS - Chat",
        WS_OVERLAPPEDWINDOW,
        x, y, width, height,
        NULL, NULL, hInstance, NULL);
    return hMainWnd;
}

// Функция для переключения видимости элементов чата
void ShowChatUI(bool show) {
    int cmd = show ? SW_SHOW : SW_HIDE;
    if (hMessageList) ShowWindow(hMessageList, cmd);
    if (hInputEdit) ShowWindow(hInputEdit, cmd);
}

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        CreateSidebar(hwnd, 0, 0, DISCORD_SIDEBAR_WIDTH, 600);
        CreateMessageList(hwnd, DISCORD_SIDEBAR_WIDTH, 0, 1000 - DISCORD_SIDEBAR_WIDTH, 550);
        CreateMessageInput(hwnd, DISCORD_SIDEBAR_WIDTH + 10, 550, 1000 - DISCORD_SIDEBAR_WIDTH - 20, INPUT_MIN_HEIGHT);
        
        // Скрываем чат при создании, так как сначала открывается меню друзей
        ShowChatUI(false); 
        break;
    }
    
    case WM_COMMAND:
        if (HIWORD(wParam) == EN_CHANGE && (HWND)lParam == hInputEdit) {
            UpdateInputHeight(hwnd, hInputEdit, hMessageList);
        }
        break;

    case WM_LBUTTONDOWN: {
        int x = LOWORD(lParam);
        int y = HIWORD(lParam);
        
        if (currentState == AppState::Friends) {
            HandleFriendsClick(hwnd, x, y); // Обработка кликов по кнопкам фильтрации и "Добавить"
        } else {
            SetFocus(hInputEdit);
        }
        break;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rect;
        GetClientRect(hwnd, &rect);
        
        if (currentState == AppState::Friends) {
            // Отрисовка страницы друзей (Верхняя панель, фильтры, список)
            DrawFriendsPage(hdc, hwnd, rect.right, rect.bottom);
        } else {
            // Отрисовка фона чата и поля ввода
            HBRUSH brush = CreateSolidBrush(COLOR_BG_DARK);
            FillRect(hdc, &rect, brush);
            DeleteObject(brush);
            DrawInputField(hdc, hwnd, hInputEdit);
        }
        
        EndPaint(hwnd, &ps);
        break;
    }
    
    case WM_SIZE: {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);
        
        if (hSidebar)
            MoveWindow(hSidebar, 0, 0, DISCORD_SIDEBAR_WIDTH, height, TRUE);
        
        if (hMessageList)
            MoveWindow(hMessageList, DISCORD_SIDEBAR_WIDTH, 0, 
                       width - DISCORD_SIDEBAR_WIDTH, height - inputEditHeight - 20, TRUE);
        
        if (hInputEdit)
            MoveWindow(hInputEdit, DISCORD_SIDEBAR_WIDTH + 15, height - inputEditHeight - 10, 
                       width - DISCORD_SIDEBAR_WIDTH - 30, inputEditHeight, TRUE);
        
        InvalidateRect(hwnd, NULL, TRUE);
        break;
    }

    // Пример переключения страницы (вызывается из Sidebar)
    case WM_USER + 1: { // Сообщение для переключения на чат
        currentState = AppState::Message;
        ShowChatUI(true);
        InvalidateRect(hwnd, NULL, TRUE);
        break;
    }
    
    case WM_DESTROY:
        isConnected = false;
        if (clientSocket != INVALID_SOCKET) closesocket(clientSocket);
        if (receiveThread.joinable()) receiveThread.join();
        PostQuitMessage(0);
        break;
        
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}