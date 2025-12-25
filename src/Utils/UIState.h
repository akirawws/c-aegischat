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
extern UIState g_uiState;