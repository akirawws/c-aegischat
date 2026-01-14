
#ifndef MESSAGE_LIST_H
#define MESSAGE_LIST_H

#include <windows.h>
#include <vector>
#include "Utils/Utils.h"

extern HWND hMessageList;
extern std::vector<Message> messages;
extern int scrollPos;

HWND CreateMessageList(HWND hParent, int x, int y, int width, int height);
int GetMessageHeight(const Message& msg, int width);
void DrawMessage(HDC hdc, const Message& msg, int y, int width);
void OnPaintMessageList(HDC hdc, int width, int height);
LRESULT CALLBACK MessageListWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

#endif

