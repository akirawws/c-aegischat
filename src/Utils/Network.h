#ifndef NETWORK_H
#define NETWORK_H

#include <winsock2.h>
#include <string>
#include <thread>

extern SOCKET clientSocket;
extern bool isConnected;
extern std::thread receiveThread;
extern std::string userName;
extern std::string userAvatar;

bool ConnectToServer(const std::string& address, const std::string& port);
void ReceiveMessages();
void ParseMessage(const std::string& msg);
void SendChatMessage(); 

#endif