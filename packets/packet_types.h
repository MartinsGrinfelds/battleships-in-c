#pragma once
#include <stdint.h>
// #define MAX_PLAYER_COUNT 16

struct GenericPacket{
    uint8_t sequence_number;
    uint8_t packet_content_size;
    uint8_t packet_type;
    uint8_t checksum;
    // Padding 4 bytes here <- Free space to use
    char* content; // Content is saved somewhere on memory 
};