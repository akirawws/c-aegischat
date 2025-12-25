#ifndef FRIENDSPAGE_H
#define FRIENDSPAGE_H
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <string>
#include <vector>
#include <mutex>

struct PendingRequest {
    std::string username;
};
extern std::vector<PendingRequest> pendingRequests;
extern std::mutex pendingMutex;

enum class FriendsFilter { All, Online, Pending };

void DrawFriendsPage(HDC hdc, HWND hwnd, int width, int height);
void HandleFriendsClick(HWND hwnd, int x, int y, HINSTANCE hInstance);
void AddUserToDMList(const std::string& username); 

#endif