#pragma once
#include <stdint.h> // uint definitions
#include <stdlib.h> // exit(), size_t, malloc

// Everything in this file describes data exchanged between server and client.
// Wiki: https://github.com/MartinsGrinfelds/battleships-in-c/wiki (Communication Protocole)
// Extensions to the wiki protocol are described in docs/PROTOCOL.md.

// Min/Max username length
#define MAX_USERNAME 20
#define MIN_USERNAME 1

// Packet types (DATA-0021)
#define PACKET_HELLO 0
#define PACKET_ACK 1
#define PACKET_MESSAGE 2
#define PACKET_STATE 3
#define PACKET_YOU_PLACE 4
#define PACKET_I_PLACE 5
#define PACKET_YOU_GO 6
#define PACKET_I_GO 7
#define PACKET_END_GAME 8

// Game status (STATE packet)
#define GAME_STATUS_LOBBY 0
#define GAME_STATUS_SHIP_PLACEMENT 1
#define GAME_STATUS_MAIN_GAME 2
#define GAME_STATUS_GAME_OVER 3

// Player status (DATA-0020)
#define PLAYER_STATUS_UNDEFINED 0
#define PLAYER_STATUS_NEW 1
#define PLAYER_STATUS_REGISTERED 2
#define PLAYER_STATUS_OBSERVER 3
#define PLAYER_STATUS_DISCONNECTED 4

// Message types (DATA-0024)
#define MESSAGE_TYPE_SYSTEM 0
#define MESSAGE_TYPE_PLAYER 1
#define MESSAGE_TYPE_TIMER 2
#define MESSAGE_TYPE_STATUS 3

// IGo actions (DATA-0035)
#define ACTION_MOVE 0
#define ACTION_SHOOT 1
#define ACTION_END_TURN 2 // Extension: player ends the turn (INTR-0017).

#define SHIP_TYPE_COUNT 5 // Ship object types are 1..SHIP_TYPE_COUNT

/// @brief DATA-0020
struct Player
{
    int socket_nr; // Only meaningful on server side. Not sent over network.
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
    uint8_t x; // Ship tail X.
    uint8_t y; // Ship tail Y.
    uint8_t rotation; // 0 - Up, 1 - Right, 2 - Down, 3 - Left (direction from tail to head).
    uint8_t team_id;
    uint8_t info; // Ship damage bitmask. Bit 0 is tail segment.
    uint8_t owner_id; // Extension: ID of player that owns the object (0 - nobody).
};

/// @brief DATA-0021
/// On the network header is: sequence_number (4 bytes), content size (4 bytes),
/// packet type (1 byte), checksum (1 byte). Numbers are in network byte order.
struct GenericPacket
{
    uint32_t sequence_number;     // 4 bytes
    size_t packet_content_size; // Sent as 4 bytes.
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
    char *content; // Content is saved somewhere on memory
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
    // 3 - Game over (extension)
    uint8_t status;
    uint8_t object_count;
    struct MapObject *map_objects;
    uint8_t player_count;
    struct Player *players;
};

/// @brief DATA-0024
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

/// @brief DATA-0032
struct YouPlacePacket
{
    uint8_t player_id;
    uint8_t object_type; //[1...5]
};

/// @brief DATA-0033
struct IPlacePacket
{
    struct MapObject object;
};

/// @brief DATA-0034 (with extensions about what player can still do this turn).
struct YouGoPacket
{
    uint8_t player_id; // Player whose turn it is. Sent to everyone.
    uint16_t turn_number;
    uint16_t max_turns;
    uint8_t shots_left;
    uint8_t seconds_left; // 0 - no turn time limit.
    uint8_t moves_left[SHIP_TYPE_COUNT]; // Index 0 is ship type 1.
};

/// @brief DATA-0035 (with extensions to know which ship acts and where it moves).
struct IGoPacket
{
    uint8_t player_id;
    uint8_t action; // 0 - Move, 1 - Shoot, 2 - End turn.
    uint8_t x; // Shoot target X.
    uint8_t y; // Shoot target Y.
    uint8_t object_type; // Ship that moves or shoots.
    uint8_t direction; // Move direction (same values as rotation).
};

/// @brief DATA-0036
struct EndGamePacket
{
    uint8_t winner_id; // 0 - no single winner player.
    uint8_t winner_team_id; // 0 - draw.
};

// Serialization functions return memory allocated by malloc. FREE AFTER USE!!!
// Deserialization functions return 0 on success and -1 if packet content is malformed.
// Strings inside deserialized packets are allocated by malloc and are always \0 terminated.

/// @brief Takes Hello packet and makes one long chunk of data so it can be used within GenericPacket.
/// @param packet Pointer to Hello packet.
/// @param final_size Pointer to serialization size after serialization.
/// @return Pointer to a serialized packet. FREE AFTER USE!!!
char *hello_packet_serialization(struct HelloPacket *packet, size_t *final_size);

/// @brief Takes serialized Hello packet and deserializes it. Free name after use.
/// @param serialized_packet Pointer to serialized Hello packet.
/// @param size Size of serialized packet.
/// @param deserialized_packet Pointer to Hello packet where to put data in.
/// @return 0 on success, -1 if malformed.
int hello_packet_deserialization(char *serialized_packet, size_t size, struct HelloPacket *deserialized_packet);

char *ack_packet_serialization(struct AckPacket *packet, size_t *final_size);
int ack_packet_deserialization(char *serialized_packet, size_t size, struct AckPacket *deserialized_packet);

/// @brief Serializes state. Players are serialized without socket number.
char *state_packet_serialization(struct StatePacket *packet, size_t *final_size);

/// @brief Deserializes state. Previously allocated map_objects and players are freed (pointers must be valid or NULL).
int state_packet_deserialization(char *serialized_packet, size_t size, struct StatePacket *deserialized_packet);

/// @brief Frees memory allocated within state packet (not the packet itself).
void free_state_packet(struct StatePacket *packet);

char *message_packet_serialization(struct MessagePacket *packet, size_t *final_size);
int message_packet_deserialization(char *serialized_packet, size_t size, struct MessagePacket *deserialized_packet);

char *you_place_packet_serialization(struct YouPlacePacket *packet, size_t *final_size);
int you_place_packet_deserialization(char *serialized_packet, size_t size, struct YouPlacePacket *deserialized_packet);

char *i_place_packet_serialization(struct IPlacePacket *packet, size_t *final_size);
int i_place_packet_deserialization(char *serialized_packet, size_t size, struct IPlacePacket *deserialized_packet);

char *you_go_packet_serialization(struct YouGoPacket *packet, size_t *final_size);
int you_go_packet_deserialization(char *serialized_packet, size_t size, struct YouGoPacket *deserialized_packet);

char *i_go_packet_serialization(struct IGoPacket *packet, size_t *final_size);
int i_go_packet_deserialization(char *serialized_packet, size_t size, struct IGoPacket *deserialized_packet);

char *end_game_packet_serialization(struct EndGamePacket *packet, size_t *final_size);
int end_game_packet_deserialization(char *serialized_packet, size_t size, struct EndGamePacket *deserialized_packet);
