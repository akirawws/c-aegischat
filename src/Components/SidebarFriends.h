// Components/SidebarFriends.h
#pragma once
#include <windows.h>
#include <vector>
#include <string>
#include "Utils/UIState.h" 

void DrawSidebarFriends(HDC hdc, HWND hwnd, int x, int y, int width, int height);
void HandleSidebarFriendsClick(HWND hwnd, int mouseX, int mouseY);
void HandleSidebarFriendsHover(HWND hwnd, int mouseX, int mouseY);
void SetDMUsers(const std::vector<DMUser>& users);
void AddUserToDMList(HWND hwnd, const std::string& username);