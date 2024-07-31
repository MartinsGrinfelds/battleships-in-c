#pragma once
#include <stdint.h> // uint definitions
#include <stdlib.h> // exit(), size_t, malloc

// Max username length
#define MAX_USERNAME 30

struct GenericPacket
{
    uint32_t sequence_number;     // 4 bytes
    uint32_t packet_content_size; // 4 bytes
    uint8_t packet_type;          // 1 byte
    uint8_t checksum;             // 1 byte
    // Padding 6 bytes here <- Free space to use
    char *content; // Pointer is 8? bytes Content is saved somewhere on memory
};

struct HelloPacket
{
    uint8_t name_length;
    char *name;
};

struct AckPacket
{
    uint8_t player_id;
    uint8_t team_id;
};


/// @brief Takes Hello packet and makes one long chunk of data so it can be used within GenericPacket.
/// @param packet Pointer to Hello packet.
/// @param final_size Pointer to serialization size after serialization.
/// @return Pointer to a serialized packet. FREE AFTER USE!!!
char *hello_packet_serialization(struct HelloPacket *packet, size_t *final_size);

/// @brief Takes serialized Hello packet and deserializes it. DO NOT MALLOC NAME!
/// @param serialized_packet Pointer to serialized Hello packet.
/// @param deserialized_packet Pointer to Hello packet where to put data in.
void hello_packet_deserialization(char *serialized_packet, struct HelloPacket *deserialized_packet);