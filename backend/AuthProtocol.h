#ifndef AUTH_PROTOCOL_H
#define AUTH_PROTOCOL_H

#include <cstdint>
#include <cstring>

// Все типы пакетов должны быть в одном месте
enum PacketType : uint8_t {
    PACKET_LOGIN = 1,
    PACKET_REGISTER = 2,
    PACKET_AUTH_RESPONSE = 3,
    PACKET_FRIEND_REQUEST = 4,
    PACKET_FRIEND_ACCEPT = 5,  // Добавили в enum
    PACKET_FRIEND_REJECT = 6   // Добавили в enum
};

#pragma pack(push, 1) 

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

// ТА САМАЯ СТРУКТУРА, КОТОРОЙ НЕ ХВАТАЛО:
struct FriendActionPacket {
    uint8_t type;            // PACKET_FRIEND_ACCEPT или REJECT
    char targetUsername[64]; // Имя того, чью заявку мы обрабатываем
};

#pragma pack(pop)

#endif