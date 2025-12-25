#ifndef AUTH_PROTOCOL_H
#define AUTH_PROTOCOL_H
#define PACKET_FRIEND_ACCEPT 5
#define PACKET_FRIEND_REJECT 6


#include <cstdint>
#include <cstring>

enum PacketType : uint8_t {
    PACKET_LOGIN = 1,
    PACKET_REGISTER = 2,
    PACKET_AUTH_RESPONSE = 3,
    PACKET_FRIEND_REQUEST = 4 // Добавили новый тип
};

#pragma pack(push, 1) // Выравнивание в 1 байт, чтобы данные не "смещались" в памяти
struct AuthPacket {
    uint8_t type;
    char username[64];
    char email[128];
    char password[128];
    bool rememberMe;
};
struct FriendActionPacket {
    uint8_t type;
    char targetUsername[32];
};

struct ResponsePacket {
    uint8_t type;
    bool success;
    char message[128];
};

// Добавляем структуру для друзей здесь
struct FriendPacket {
    uint8_t type;
    char targetUsername[64];
    char senderUsername[64];
};
#pragma pack(pop)

#endif