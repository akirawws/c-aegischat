#pragma once
#include <string>
enum class AppPage {
    Friends,
    Chat,
    Messages,
    FriendRequests
};

struct UIState {
    AppPage currentPage;
    std::string activeChatUser;
};
struct DMUser {
    std::string username;
    std::string id; 
    bool online;
    long long lastMessageTime;
};

extern UIState g_uiState;