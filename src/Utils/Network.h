#ifndef NETWORK_H
#define NETWORK_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h> 
#include <windows.h>
#include <string>
#include <thread>
#include <vector>
#include <Pages/FriendsPage.h>
#include "Utils/Utils.h" 
#include <map>


extern SOCKET clientSocket;
extern bool isConnected;
extern std::thread receiveThread;
extern std::string userName;
extern std::string currentUserName;
extern std::string userAvatar;
extern std::map<std::string, ChatCache> chatHistories; 
extern std::vector<Message> messages;  
extern HWND ghWnd;

bool ConnectToServer(const std::string& address, const std::string& port);
void ReceiveMessages();
void ParseMessage(const std::string& msg);
void SendChatMessage(); 
void StartMessageSystem();
void SendPrivateMessageFromUI();
bool SendPacket(const char* data, int size);
bool ReceivePacket(char* data, int size);
void RequestCreateGroup(const std::vector<std::string>& members);
void SendDisplayNameChange(const std::string& newDisplayName);
void SendAvatarUpdate(const std::wstring& filePath);
void SendBioChange(const std::string& newBio);

#endif