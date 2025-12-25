#pragma once
#include <windows.h>
#include <vector>
#include <string>

struct DMUser {
    std::string username;
    bool online;
};

void DrawSidebarFriends(
    HDC hdc,
    HWND hwnd,
    int x,
    int y,
    int width,
    int height
);

void HandleSidebarFriendsClick(
    HWND hwnd,
    int mouseX,
    int mouseY
);

void HandleSidebarFriendsHover(
    HWND hwnd,
    int mouseX,
    int mouseY
);

void SetDMUsers(const std::vector<DMUser>& users);
