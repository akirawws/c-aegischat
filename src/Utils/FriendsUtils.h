#pragma once
#include <string>
#include <Windows.h>

// Результат модального окна добавления друга
struct AddFriendModalResult {
    bool confirmed = false;
    std::string username;
};

// Показать модальное окно для ввода username
AddFriendModalResult ShowAddFriendModal(HINSTANCE hInstance, HWND parent);

// Работа с друзьями (отправка/принятие/отклонение заявок)
bool SendFriendRequest(const std::string& username);
bool AcceptFriendRequest(const std::string& username);
bool RejectFriendRequest(const std::string& username);

// Обработчик кликов по заявкам (опционально)
void HandlePendingRequestClick(HWND hwnd, int x, int y);
