#pragma once
#include <stdint.h> // uint definitions

struct GenericPacket
{
    uint32_t sequence_number;     // 4 bytes
    uint32_t packet_content_size; // 4 bytes
    uint8_t packet_type;          // 1 byte
    uint8_t checksum;             // 1 byte
    // Padding 6 bytes here <- Free space to use
    char *content; // Content is saved somewhere on memory
};