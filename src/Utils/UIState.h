#pragma once
#include <string>
#include <vector>

enum class AppPage {
    Friends,
    Chat,
    Messages,
    FriendRequests
};

struct UIState {
    AppPage currentPage;
    std::string activeChatUser;
    

    std::string userDisplayName;
    std::string userAvatarUrl;
};
struct DMUser {
    std::string username;
    std::string id; 
    bool online;
    long long lastMessageTime;
};

extern UIState g_uiState;