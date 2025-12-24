#include "FriendsUtils.h"
#include "Utils/Styles.h"
#include "AuthProtocol.h"
#include "Utils/Network.h"

// Глобальная переменная или extern, где хранится имя текущего пользователя после логина
// Тебе нужно убедиться, что при логине ты записываешь имя в эту переменную
extern std::string currentUserName; 

struct ModalContext {
    AddFriendModalResult* res;
    HWND hEdit;
    bool finished = false;
};

LRESULT CALLBACK AddFriendWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    ModalContext* ctx = (ModalContext*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, COLOR_TEXT_WHITE);
        SetBkColor(hdc, RGB(30, 31, 34));
        static HBRUSH hEditBg = CreateSolidBrush(RGB(30, 31, 34));
        return (LRESULT)hEditBg;
    }
    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
        bool isOk = (dis->CtlID == 2);
        COLORREF color = isOk ? RGB(36, 128, 70) : RGB(78, 80, 88);
        if (dis->itemState & ODS_SELECTED) color = isOk ? RGB(26, 99, 52) : RGB(60, 62, 68);

        HBRUSH hBrush = CreateSolidBrush(color);
        FillRect(dis->hDC, &dis->rcItem, hBrush);
        DeleteObject(hBrush);
        SetTextColor(dis->hDC, RGB(255, 255, 255));
        SetBkMode(dis->hDC, TRANSPARENT);
        char text[64]; GetWindowTextA(dis->hwndItem, text, 64);
        DrawTextA(dis->hDC, text, -1, &dis->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return TRUE;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT r; GetClientRect(hwnd, &r);
        HBRUSH hBg = CreateSolidBrush(COLOR_BG_DARK);
        FillRect(hdc, &r, hBg);
        DeleteObject(hBg);
        SetTextColor(hdc, RGB(181, 186, 193));
        SetBkMode(hdc, TRANSPARENT);
        HFONT hF = CreateAppFont(16, FONT_WEIGHT_BOLD);
        SelectObject(hdc, hF);
        TextOutA(hdc, 20, 15, "ADD FRIEND", 10);
        DeleteObject(hF);
        EndPaint(hwnd, &ps);
        break;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == 2) { // Кнопка ADD
            char buffer[64];
            GetWindowTextA(ctx->hEdit, buffer, 64);
            ctx->res->username = buffer;
            ctx->res->confirmed = !ctx->res->username.empty();
            ctx->finished = true;
        }
        if (LOWORD(wParam) == 3) {
            ctx->res->confirmed = false;
            ctx->finished = true;
        }
        break;
    case WM_CLOSE:
        ctx->finished = true;
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

AddFriendModalResult ShowAddFriendModal(HINSTANCE hInstance, HWND parent) {
    AddFriendModalResult result = { false, "" };
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = AddFriendWndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "AegisModalClass";
    RegisterClassA(&wc);

    HWND hwndDlg = CreateWindowExA(WS_EX_TOPMOST, "AegisModalClass", "Add Friend",
        WS_POPUP | WS_CAPTION | WS_SYSMENU, 0, 0, 360, 220, parent, NULL, hInstance, NULL);

    RECT pr; GetWindowRect(parent, &pr);
    SetWindowPos(hwndDlg, NULL, pr.left + (pr.right-pr.left-360)/2, pr.top + (pr.bottom-pr.top-220)/2, 0, 0, SWP_NOSIZE);

    HWND hEdit = CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                               20, 60, 305, 35, hwndDlg, (HMENU)1, hInstance, NULL);
    
    CreateWindowA("BUTTON", "Send Friend Request", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                  20, 110, 305, 45, hwndDlg, (HMENU)2, hInstance, NULL);
    
    CreateWindowA("BUTTON", "Cancel", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                  20, 165, 305, 30, hwndDlg, (HMENU)3, hInstance, NULL);

    ModalContext ctx = { &result, hEdit };
    SetWindowLongPtr(hwndDlg, GWLP_USERDATA, (LONG_PTR)&ctx);
    EnableWindow(parent, FALSE);
    ShowWindow(hwndDlg, SW_SHOW);

    MSG msg;
    while (!ctx.finished && GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    EnableWindow(parent, TRUE);
    SetFocus(parent);
    DestroyWindow(hwndDlg);
    return result;
}

bool SendFriendRequest(const std::string& username) {
    if (username.empty()) return false;

    FriendPacket packet;
    memset(&packet, 0, sizeof(FriendPacket));
    packet.type = PACKET_FRIEND_REQUEST;
    
    // Копируем имя того, КОМУ шлем
    strncpy(packet.targetUsername, username.c_str(), sizeof(packet.targetUsername) - 1);
    
    // Копируем имя того, КТО шлет (нас)
    strncpy(packet.senderUsername, currentUserName.c_str(), sizeof(packet.senderUsername) - 1);

    // Отправляем на сервер
    return send(clientSocket, (char*)&packet, sizeof(FriendPacket), 0) != SOCKET_ERROR;
}

bool AcceptFriendRequest(const std::string& username) {
    // Здесь логика будет похожей: отправить пакет PACKET_FRIEND_ACCEPT
    return true;
}

bool RejectFriendRequest(const std::string& username) {
    return true;
}