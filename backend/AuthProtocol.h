#ifndef AUTH_PROTOCOL_H
#define AUTH_PROTOCOL_H

#include <cstdint>
#include <cstring>

enum PacketType : uint8_t {
    PACKET_LOGIN = 1,
    PACKET_REGISTER = 2,
    PACKET_AUTH_RESPONSE = 3
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
#pragma pack(pop)

#endif