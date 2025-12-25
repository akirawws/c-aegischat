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
    PACKET_CHAT_MESSAGE = 8    
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
    char username[64]; // Сделал 64 для единообразия
};

struct ChatMessagePacket {
    uint8_t type;
    char senderUsername[64];   
    char targetUsername[64];  
    char content[384];       
};


#pragma pack(pop)
#endif