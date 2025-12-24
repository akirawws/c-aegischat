#ifndef MESSAGE_INPUT_H
#define MESSAGE_INPUT_H

#include <windows.h>

extern HWND hInputEdit;
extern int inputEditHeight;

const int INPUT_MIN_HEIGHT = 60;
const int INPUT_MAX_HEIGHT = 200;

HWND CreateMessageInput(HWND hParent, int x, int y, int width, int height);
void UpdateInputHeight(HWND hwnd, HWND hInputEdit, HWND hMessageList);
void DrawInputField(HDC hdc, HWND hwnd, HWND hInputEdit);
void SendChatMessage();

#endif

