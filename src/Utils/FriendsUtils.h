#pragma once
#include <string>
#include <Windows.h>

struct AddFriendModalResult {
    bool confirmed = false;
    std::string username;
};

AddFriendModalResult ShowAddFriendModal(HINSTANCE hInstance, HWND parent);
bool SendFriendRequest(const std::string& username);
bool AcceptFriendRequest(const std::string& username);
bool RejectFriendRequest(const std::string& username);
void HandlePendingRequestClick(HWND hwnd, int x, int y);
