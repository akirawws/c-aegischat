#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <Utils/Utils.h>

void DrawMessagePage(HDC hdc, int x, int y, int w, int h, const std::vector<Message>& messages);
void DrawMessageInput(HDC hdc, int x, int y, int w, int h, const std::string& inputText);
