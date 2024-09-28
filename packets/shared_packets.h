#pragma once
#include <stdint.h> // uint definitions
#include <stdlib.h> // exit(), size_t, malloc

// Min/Max username length
#define MAX_USERNAME 20
#define MIN_USERNAME 1


/// @brief DATA-0020
struct Player
{
    int socket_nr;
    uint8_t id;
    uint8_t team_id;
    uint8_t status;
    char username[MAX_USERNAME];
};

/// @brief DATA-0021
struct GenericPacket
{
    uint32_t sequence_number;     // 4 bytes
    size_t packet_content_size; // ? bytes
    uint8_t packet_type;          // 1 byte
    uint8_t checksum;             // 1 byte
    // Padding 6 bytes here <- Free space to use
    char *content; // Pointer is 8? bytes Content is saved somewhere on memory
};

/// @brief DATA-0022
struct HelloPacket
{
    uint8_t name_length;
    char *name;
};

/// @brief DATA-0023
struct AckPacket
{
    uint8_t player_id;
    uint8_t team_id;
    uint8_t message_length;
    char* message;
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

/// @brief Takes ACK packet and makes one long chunk of data so it can be used within GenericPacket.
/// @param packet Pointer to ACK packet.
/// @param final_size Pointer to serialization size after serialization.
/// @return Pointer to a serialized packet. FREE AFTER USE!!!
char *ack_packet_serialization(struct AckPacket *packet, size_t *final_size);

/// @brief Takes serialized ACK packet and deserializes it.
/// @param serialized_packet Pointer to serialized ACK packet.
/// @param deserialized_packet Pointer to ACK packet where to put data in.
void ack_packet_deserialization(char *serialized_packet, struct AckPacket *deserialized_packet);