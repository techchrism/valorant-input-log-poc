#ifndef VALORANT_INPUT_LOG_FORMAT_TYPES_H
#define VALORANT_INPUT_LOG_FORMAT_TYPES_H

#define LOG_HEADER "valorant-input-log"
#define LOG_FORMAT_VERSION 1

enum PacketType : unsigned char
{
    KEY_DOWN = 0,
    KEY_UP,
    MOUSE_BUTTON,
    MOUSE_MOVE_TINY,
    MOUSE_MOVE_SMALL,
    MOUSE_MOVE_MED
};

#pragma pack(1)
struct PacketHeader
{
    PacketType type;
    __int64 timestamp;
};

struct PacketMouseButton
{
    USHORT buttonData;
};

#pragma pack(1)
struct PacketMouseMoveTiny
{
    unsigned short totalInputs;
    __int8 x, y;
};

#pragma pack(1)
struct PacketMouseMoveSmall
{
    unsigned short totalInputs;
    __int16 x, y;
};

#pragma pack(1)
struct PacketMouseMoveMed
{
    unsigned short totalInputs;
    __int32 x, y;
};

struct PacketKeyboard
{
    unsigned char virtualCode;
};

#pragma pack(1)
struct InputPacket
{
    PacketHeader header;
    union
    {
        PacketKeyboard keyboard;
        PacketMouseButton mouseButton;
        PacketMouseMoveTiny mouseMoveTiny;
        PacketMouseMoveSmall mouseMoveSmall;
        PacketMouseMoveMed mouseMoveMed;
    };
};

#endif //VALORANT_INPUT_LOG_FORMAT_TYPES_H
