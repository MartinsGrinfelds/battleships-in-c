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
    // Team ID (1 byte):
    // 0 - Observer
    // >0 - Team number
    uint8_t team_id;
    // Status:
    // 0 - undefined
    // 1 - new, to register name
    // 2 - registered
    // 3 - observer
    // 4 - disconnected mid-game or in lobby
    uint8_t status;
    char username[MAX_USERNAME + 1]; // +1 for /0 symbol.
};

/// @brief DATA-0019, BIC-59
struct MapObject
{
    uint8_t object_type;
    uint8_t x;
    uint8_t y;
    uint8_t rotation;
    uint8_t team_id;
    uint8_t info;
};

/// @brief Partially DATA-0019, BIC-59 but to represent objects in server for easy manipulation.
struct MapObjects
{
    struct MapObject object;
    struct MapObject *next_object;
};

/// @brief DATA-0021
struct GenericPacket
{
    uint32_t sequence_number;     // 4 bytes
    size_t packet_content_size; // ? bytes
    // Packet type (1 byte):
    // 0 - Hello packet
    // 1 - ACK packet
    // 2 - Message packet
    // 3 - State packet
    // 4 - YouPlace packet
    // 5 - IPlace packet
    // 6 - YouGo packet
    // 7 - IGo packet
    // 8 - EndGame packet
    uint8_t packet_type;
    uint8_t checksum;             // 1 byte
    // Padding ? bytes here <- Free space to use
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

/// @brief DATA-0031, BIC-66
struct StatePacket
{
    uint8_t map_width;
    uint8_t map_height;
    // Game status (1 byte):
    // 0 - Lobby, waiting for players
    // 1 - Ship placement
    // 2 - Main game
    uint8_t status;
    uint8_t object_count;
    struct MapObject *map_objects;
    uint8_t player_count;
    struct Player *players;
};

struct MessagePacket
{
    // Message type (1 byte):
    // 0 - Global system message
    // 1 - Player message
    // 2 - Timer message
    // 3 - Player status message. Hits in target, hits missed, points, options, etc.
    uint8_t message_type;
    uint8_t sender_id;
    uint8_t receiver_id;
    uint8_t message_length;
    char *message;
};

struct YouPlacePacket
{
    uint8_t player_id;
    uint8_t object_type; //[1...5]
};
struct IPlacePacket
{
    struct MapObject object;
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

/// @brief Takes Player packet and makes one long chunk of data so it can be used within GenericPacket.
/// @param packet Pointer to Player packet.
/// @param final_size Pointer to serialization size after serialization.
/// @return Pointer to a serialized packet. FREE AFTER USE!!!
char *player_packet_serialization(struct Player *packet, size_t *final_size);

/// @brief Takes serialized Player packet and deserializes it.
/// @param serialized_packet Pointer to serialized Player packet.
/// @param deserialized_packet Pointer to Player packet where to put data in.
void player_packet_deserialization(char *serialized_packet, struct Player *deserialized_packet);

/// @brief Takes State packet and makes one long chunk of data so it can be used within GenericPacket.
/// @param packet Pointer to State packet.
/// @param final_size Pointer to serialization size after serialization.
/// @return Pointer to a serialized packet. FREE AFTER USE!!!
char *state_packet_serialization(struct StatePacket *packet, size_t *final_size);

/// @brief Takes serialized State packet and deserializes it.
/// @param serialized_packet Pointer to serialized State packet.
/// @param deserialized_packet Pointer to State packet where to put data in.
void state_packet_deserialization(char *serialized_packet, struct StatePacket *deserialized_packet);

/// @brief Takes Message packet and makes one long chunk of data so it can be used within GenericPacket.
/// @param packet Pointer to Message packet.
/// @param final_size Pointer to serialization size after serialization.
/// @return Pointer to a serialized packet. FREE AFTER USE!!!
char *message_packet_serialization(struct MessagePacket *packet, size_t *final_size);

/// @brief Takes serialized Message packet and deserializes it.
/// @param serialized_packet Pointer to serialized Message packet.
/// @param deserialized_packet Pointer to Mesaage packet where to put data in.
void message_packet_deserialization(char *serialized_packet, struct MessagePacket *deserialized_packet);

/// @brief Takes YouPlace packet and makes one long chunk of data so it can be used within GenericPacket.
/// @param packet Pointer to YouPlace packet.
/// @param final_size Pointer to serialization size after serialization.
/// @return Pointer to a serialized packet. FREE AFTER USE!!!
char *you_place_packet_serialization(struct YouPlacePacket *packet, size_t *final_size);

/// @brief Takes serialized YouPlace packet and deserializes it.
/// @param serialized_packet Pointer to serialized YouPlace packet.
/// @param deserialized_packet Pointer to YouPlace packet where to put data in.
void you_place_packet_deserialization(char *serialized_packet, struct YouPlacePacket *deserialized_packet);

/// @brief Takes IPlace packet and makes one long chunk of data so it can be used within GenericPacket.
/// @param packet Pointer to IPlace packet.
/// @param final_size Pointer to serialization size after serialization.
/// @return Pointer to a serialized packet. FREE AFTER USE!!!
char *i_place_packet_serialization(struct IPlacePacket *packet, size_t *final_size);

/// @brief Takes serialized IPlace packet and deserializes it.
/// @param serialized_packet Pointer to serialized IPlace packet.
/// @param deserialized_packet Pointer to IPlace packet where to put data in.
void i_place_packet_deserialization(char *serialized_packet, struct IPlacePacket *deserialized_packet);
