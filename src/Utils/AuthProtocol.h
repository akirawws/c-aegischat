#ifndef AUTH_PROTOCOL_H
#define AUTH_PROTOCOL_H

#include <cstdint>

#pragma pack(push, 1) 

enum PacketType : uint8_t {
    PACKET_LOGIN = 1,
    PACKET_REGISTER = 2,
    PACKET_AUTH_RESPONSE = 3,
    PACKET_FRIEND_REQUEST = 4,
    PACKET_FRIEND_ACCEPT = 5,
    PACKET_FRIEND_REJECT = 6,
    PACKET_ROOM_LIST = 7,      
    PACKET_CHAT_MESSAGE = 8,
    PACKET_USER_STATUS = 9,
    PACKET_CHAT_HISTORY = 10,
    PACKET_CREATE_GROUP = 11,
    PACKET_GROUP_LIST = 12,
    PACKET_USER_PROFILE = 13
};

struct AuthPacket {
    uint8_t type;
    char username[64];
    char email[128];
    char password[128];
    bool rememberMe;
};

struct ResponsePacket {
    uint8_t type;
    bool success;
    char message[128];
};

struct FriendPacket {
    uint8_t type;
    char targetUsername[64];
    char senderUsername[64];
};

struct FriendActionPacket {
    uint8_t type;
    char targetUsername[64];
};

struct RoomPacket {
    uint8_t type;
    char username[64]; 
    uint8_t onlineStatus;
};
struct ChatMessagePacket {
    uint8_t type;
    char senderUsername[64];   
    char targetUsername[64];  
    char content[512]; 
};

struct UserStatusPacket {
    uint8_t type;
    char username[64];
    uint8_t onlineStatus;    
};

struct HistoryRequestPacket {
    uint8_t type;
    char targetUsername[64];
    int offset; 
};
struct ChatHistoryEntryPacket {
    uint8_t type;
    char senderUsername[64];
    char content[512];
    char timestamp[32]; 
    bool isLast;      
};

struct CreateGroupPacket {
    uint8_t type;
    char groupName[64];
    int userCount;
    char members[10][64]; 
};
struct UserProfilePacket {
    uint8_t type;
    char username[64];
    char display_name[64];
    char avatar_url[256];
};

#pragma pack(pop)
#endif