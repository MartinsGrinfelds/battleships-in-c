#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>     // write(), close()
#include <sys/socket.h> // socketpair()
#include <arpa/inet.h>  // htonl()
#include "../packets/connection.h"
#include "../packets/shared_packets.h"
#include "../game/game_logic.h"
#include "../graphical/text_formatter.h"
#include "../utils/config.h"

// Small test framework: CHECK prints failure and test continues.
static int checks = 0, failures = 0;

#define CHECK(condition)                                                        \
    do                                                                          \
    {                                                                           \
        checks++;                                                               \
        if (!(condition))                                                       \
        {                                                                       \
            failures++;                                                         \
            printf("  FAILED %s:%d: %s\n", __FILE__, __LINE__, #condition);     \
        }                                                                       \
    } while (0)

#define CHECK_EQ(actual, expected)                                                          \
    do                                                                                      \
    {                                                                                       \
        checks++;                                                                           \
        long long actual_value = (long long)(actual), expected_value = (long long)(expected); \
        if (actual_value != expected_value)                                                 \
        {                                                                                   \
            failures++;                                                                     \
            printf("  FAILED %s:%d: %s is %lld, expected %lld\n", __FILE__, __LINE__,       \
                   #actual, actual_value, expected_value);                                  \
        }                                                                                   \
    } while (0)

static struct MapObject make_ship(uint8_t type, int x, int y, uint8_t rotation, uint8_t team, uint8_t owner)
{
    struct MapObject ship;
    memset(&ship, 0, sizeof(ship));
    ship.object_type = type;
    ship.x = (uint8_t)x;
    ship.y = (uint8_t)y;
    ship.rotation = rotation;
    ship.team_id = team;
    ship.owner_id = owner;
    return ship;
}

// ---------------------------------------------------------------------------
// Packets
// ---------------------------------------------------------------------------

static void test_simple_packets()
{
    printf("Packet serialization round trips\n");
    size_t size;
    char *data;

    struct HelloPacket hello = {5, "Alice"}, hello_out;
    data = hello_packet_serialization(&hello, &size);
    CHECK_EQ(size, 6);
    CHECK_EQ(hello_packet_deserialization(data, size, &hello_out), 0);
    CHECK(strcmp(hello_out.name, "Alice") == 0);
    free(hello_out.name);
    // Truncated packet must be rejected and never read outside.
    CHECK_EQ(hello_packet_deserialization(data, 3, &hello_out), -1);
    free(hello_out.name);
    CHECK_EQ(hello_packet_deserialization(data, 0, &hello_out), -1);
    free(hello_out.name);
    free(data);

    struct AckPacket ack = {7, 2, 3, "Hey"}, ack_out;
    data = ack_packet_serialization(&ack, &size);
    CHECK_EQ(ack_packet_deserialization(data, size, &ack_out), 0);
    CHECK_EQ(ack_out.player_id, 7);
    CHECK_EQ(ack_out.team_id, 2);
    CHECK(strcmp(ack_out.message, "Hey") == 0);
    free(ack_out.message);
    free(data);

    // NULL message is sent as empty.
    struct AckPacket empty_ack = {1, 1, 10, NULL};
    data = ack_packet_serialization(&empty_ack, &size);
    CHECK_EQ(size, 3);
    CHECK_EQ(ack_packet_deserialization(data, size, &ack_out), 0);
    CHECK_EQ(ack_out.message_length, 0);
    CHECK(strcmp(ack_out.message, "") == 0);
    free(ack_out.message);
    free(data);

    struct MessagePacket message = {MESSAGE_TYPE_PLAYER, 3, 0, 5, "Hello"}, message_out;
    data = message_packet_serialization(&message, &size);
    CHECK_EQ(message_packet_deserialization(data, size, &message_out), 0);
    CHECK_EQ(message_out.sender_id, 3);
    CHECK(strcmp(message_out.message, "Hello") == 0);
    free(message_out.message);
    CHECK_EQ(message_packet_deserialization(data, size - 1, &message_out), -1);
    free(message_out.message);
    free(data);

    struct YouPlacePacket you_place = {4, 3}, you_place_out;
    data = you_place_packet_serialization(&you_place, &size);
    CHECK_EQ(you_place_packet_deserialization(data, size, &you_place_out), 0);
    CHECK_EQ(you_place_out.object_type, 3);
    free(data);

    struct IPlacePacket i_place, i_place_out;
    i_place.object = make_ship(2, 10, 11, ROTATION_LEFT, 1, 4);
    data = i_place_packet_serialization(&i_place, &size);
    CHECK_EQ(size, 7);
    CHECK_EQ(i_place_packet_deserialization(data, size, &i_place_out), 0);
    CHECK(memcmp(&i_place.object, &i_place_out.object, sizeof(struct MapObject)) == 0);
    free(data);

    struct YouGoPacket you_go = {9, 300, 150, 1, 42, {0, 1, 3, 2, 5}}, you_go_out;
    data = you_go_packet_serialization(&you_go, &size);
    CHECK_EQ(you_go_packet_deserialization(data, size, &you_go_out), 0);
    CHECK_EQ(you_go_out.turn_number, 300); // Two byte number in network byte order.
    CHECK_EQ((uint8_t)data[1], 1);
    CHECK_EQ((uint8_t)data[2], 44);
    CHECK_EQ(you_go_out.moves_left[2], 3);
    CHECK_EQ(you_go_out.seconds_left, 42);
    free(data);

    struct IGoPacket i_go = {1, ACTION_SHOOT, 33, 44, 2, 0}, i_go_out;
    data = i_go_packet_serialization(&i_go, &size);
    CHECK_EQ(i_go_packet_deserialization(data, size, &i_go_out), 0);
    CHECK_EQ(i_go_out.x, 33);
    CHECK_EQ(i_go_out.y, 44);
    CHECK_EQ(i_go_packet_deserialization(data, 2, &i_go_out), -1);
    free(data);

    struct EndGamePacket end_game = {5, 2}, end_game_out;
    data = end_game_packet_serialization(&end_game, &size);
    CHECK_EQ(end_game_packet_deserialization(data, size, &end_game_out), 0);
    CHECK_EQ(end_game_out.winner_team_id, 2);
    free(data);
}

static void test_state_packet()
{
    printf("State packet\n");
    struct MapObject objects[3] = {make_ship(1, 1, 2, 0, 1, 1), make_ship(5, 40, 41, 3, 2, 2), make_ship(14, 7, 8, 0, 1, 0)};
    objects[1].info = 1;
    struct Player players[2];
    memset(players, 0, sizeof(players));
    players[0].id = 1;
    players[0].team_id = 1;
    players[0].status = PLAYER_STATUS_REGISTERED;
    players[0].socket_nr = 99;
    strcpy(players[0].username, "Martins");
    players[1].id = 2;
    players[1].team_id = 2;
    players[1].status = PLAYER_STATUS_DISCONNECTED;
    strcpy(players[1].username, "Twenty Characters Ok");
    struct StatePacket state = {50, 60, GAME_STATUS_MAIN_GAME, 3, objects, 2, players};
    size_t size;
    char *data = state_packet_serialization(&state, &size);
    struct StatePacket state_out;
    memset(&state_out, 0, sizeof(state_out));
    CHECK_EQ(state_packet_deserialization(data, size, &state_out), 0);
    CHECK_EQ(state_out.map_width, 50);
    CHECK_EQ(state_out.map_height, 60);
    CHECK_EQ(state_out.object_count, 3);
    CHECK(memcmp(state_out.map_objects, objects, sizeof(objects)) == 0);
    CHECK_EQ(state_out.player_count, 2);
    CHECK(strcmp(state_out.players[1].username, "Twenty Characters Ok") == 0);
    CHECK_EQ(state_out.players[0].socket_nr, -1); // Socket number is not sent.
    // Receiving again frees old data (checked by sanitizer).
    CHECK_EQ(state_packet_deserialization(data, size, &state_out), 0);
    // Every truncated version must be rejected.
    int all_rejected = 1;
    size_t length = 0;
    while (length < size)
    {
        if (state_packet_deserialization(data, length++, &state_out) == 0)
        {
            all_rejected = 0;
        }
    }
    CHECK(all_rejected);
    CHECK(state_out.map_objects == NULL);
    free_state_packet(&state_out);
    free(data);

    // Empty state.
    struct StatePacket empty = {40, 40, GAME_STATUS_LOBBY, 0, NULL, 0, NULL};
    data = state_packet_serialization(&empty, &size);
    CHECK_EQ(size, 5);
    CHECK_EQ(state_packet_deserialization(data, size, &state_out), 0);
    CHECK_EQ(state_out.object_count, 0);
    free_state_packet(&state_out);
    free(data);
}

// ---------------------------------------------------------------------------
// Connection
// ---------------------------------------------------------------------------

static void make_connection_pair(struct Connection *a, struct Connection *b)
{
    int sockets[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, sockets);
    set_socket_non_blocking(sockets[0]);
    set_socket_non_blocking(sockets[1]);
    connection_init(a, sockets[0]);
    connection_init(b, sockets[1]);
}

/// @brief Builds raw packet bytes like peer would send them.
static size_t raw_packet(char *buffer, uint32_t sequence, uint8_t type, const char *content, uint32_t size, int break_checksum)
{
    uint32_t value = htonl(sequence);
    memcpy(buffer, &value, 4);
    value = htonl(size);
    memcpy(buffer + 4, &value, 4);
    buffer[8] = (char)type;
    buffer[9] = (char)(calculate_checksum(sequence, size, type, content) ^ (break_checksum ? 0x5A : 0));
    memcpy(buffer + 10, content, size);
    return 10 + size;
}

static void test_connection()
{
    printf("Connection framing\n");
    struct Connection a, b;
    make_connection_pair(&a, &b);

    // Several packets at once are split correctly.
    CHECK_EQ(connection_send(&a, PACKET_MESSAGE, "one", 3), CONNECTION_OK);
    CHECK_EQ(connection_send(&a, PACKET_STATE, "", 0), CONNECTION_OK);
    CHECK_EQ(connection_send(&a, PACKET_ACK, "three", 5), CONNECTION_OK);
    CHECK_EQ(connection_read(&b), CONNECTION_OK);
    struct GenericPacket *packet = connection_next_packet(&b);
    CHECK(packet && packet->packet_type == PACKET_MESSAGE && packet->packet_content_size == 3 && memcmp(packet->content, "one", 3) == 0);
    CHECK(packet && packet->sequence_number == 1);
    free_generic_packet(packet);
    packet = connection_next_packet(&b);
    CHECK(packet && packet->packet_type == PACKET_STATE && packet->packet_content_size == 0);
    free_generic_packet(packet);
    packet = connection_next_packet(&b);
    CHECK(packet && packet->packet_type == PACKET_ACK && packet->sequence_number == 3);
    free_generic_packet(packet);
    CHECK(connection_next_packet(&b) == NULL);

    // Packet arriving one byte at a time is assembled.
    char raw[64];
    size_t raw_size = raw_packet(raw, 10, PACKET_HELLO, "\x03" "Bob", 4, 0);
    size_t i = 0;
    int early_packet = 0;
    while (i < raw_size)
    {
        CHECK_EQ(write(a.socket, raw + i, 1), 1);
        connection_read(&b);
        if (i + 1 < raw_size && connection_next_packet(&b) != NULL)
        {
            early_packet = 1;
        }
        i++;
    }
    CHECK(!early_packet);
    packet = connection_next_packet(&b);
    CHECK(packet && packet->packet_type == PACKET_HELLO && packet->sequence_number == 10);
    free_generic_packet(packet);

    // Wrong checksum is dropped but next packet is fine.
    raw_size = raw_packet(raw, 11, PACKET_MESSAGE, "bad", 3, 1);
    CHECK_EQ(write(a.socket, raw, raw_size), (ssize_t)raw_size);
    raw_size = raw_packet(raw, 12, PACKET_MESSAGE, "good", 4, 0);
    CHECK_EQ(write(a.socket, raw, raw_size), (ssize_t)raw_size);
    connection_read(&b);
    packet = connection_next_packet(&b);
    CHECK(packet && packet->sequence_number == 12 && memcmp(packet->content, "good", 4) == 0);
    free_generic_packet(packet);
    CHECK_EQ(b.dropped_packets, 1);

    // Old sequence number is dropped. 0 is allowed (counter overflow).
    raw_size = raw_packet(raw, 5, PACKET_MESSAGE, "old", 3, 0);
    CHECK_EQ(write(a.socket, raw, raw_size), (ssize_t)raw_size);
    raw_size = raw_packet(raw, 0, PACKET_MESSAGE, "zero", 4, 0);
    CHECK_EQ(write(a.socket, raw, raw_size), (ssize_t)raw_size);
    connection_read(&b);
    packet = connection_next_packet(&b);
    CHECK(packet && packet->sequence_number == 0);
    free_generic_packet(packet);
    CHECK_EQ(b.dropped_packets, 2);

    // Sender sequence number overflows to 0.
    a.send_sequence = UINT32_MAX;
    connection_send(&a, PACKET_ACK, "x", 1);
    connection_send(&a, PACKET_ACK, "y", 1);
    connection_send(&a, PACKET_ACK, "z", 1);
    connection_read(&b);
    int received = 0;
    while ((packet = connection_next_packet(&b)) != NULL)
    {
        received++;
        free_generic_packet(packet);
    }
    CHECK_EQ(received, 3);
    CHECK_EQ(b.dropped_packets, 2);

    // Too big content size breaks stream.
    raw_size = raw_packet(raw, 20, PACKET_MESSAGE, "", 0, 0);
    uint32_t huge = htonl(MAX_CONTENT_SIZE + 1);
    memcpy(raw + 4, &huge, 4);
    CHECK_EQ(write(a.socket, raw, raw_size), (ssize_t)raw_size);
    connection_read(&b);
    CHECK(connection_next_packet(&b) == NULL);
    CHECK_EQ(b.state, CONNECTION_PROTOCOL_ERROR);

    connection_close(&a);
    connection_close(&b);

    // Peer closing connection is noticed and data sent before closing is still delivered.
    make_connection_pair(&a, &b);
    connection_send(&a, PACKET_END_GAME, "\x01\x02", 2);
    connection_close(&a);
    CHECK_EQ(connection_read(&b), CONNECTION_CLOSED);
    packet = connection_next_packet(&b);
    CHECK(packet && packet->packet_type == PACKET_END_GAME);
    free_generic_packet(packet);
    CHECK(connection_next_packet(&b) == NULL);
    // Sending to closed connection fails without crash (SIGPIPE).
    CHECK(connection_send(&b, PACKET_ACK, "x", 1) != CONNECTION_OK || connection_flush(&b) != CONNECTION_OK);
    connection_close(&b);

    // Big packet that doesn't fit into socket buffer at once is delivered in pieces.
    make_connection_pair(&a, &b);
    char *big = malloc(MAX_CONTENT_SIZE);
    i = 0;
    while (i < MAX_CONTENT_SIZE)
    {
        big[i] = (char)(i * 7);
        i++;
    }
    CHECK_EQ(connection_send(&a, PACKET_STATE, big, MAX_CONTENT_SIZE), CONNECTION_OK);
    packet = NULL;
    int rounds = 0;
    while (!packet && rounds++ < 10000)
    {
        connection_flush(&a);
        connection_read(&b);
        packet = connection_next_packet(&b);
    }
    CHECK(packet && packet->packet_content_size == MAX_CONTENT_SIZE && memcmp(packet->content, big, MAX_CONTENT_SIZE) == 0);
    free_generic_packet(packet);
    free(big);

    // Testing knob: chunked sending still delivers everything.
    a.send_chunk_limit = 2;
    connection_send(&a, PACKET_MESSAGE, "chunks", 6);
    packet = NULL;
    rounds = 0;
    while (!packet && rounds++ < 100)
    {
        connection_flush(&a);
        connection_read(&b);
        packet = connection_next_packet(&b);
    }
    CHECK(packet && memcmp(packet->content, "chunks", 6) == 0);
    CHECK(rounds > 5);
    free_generic_packet(packet);
    connection_close(&a);
    connection_close(&b);
}

// ---------------------------------------------------------------------------
// Game logic
// ---------------------------------------------------------------------------

static void test_ship_geometry()
{
    printf("Ship geometry\n");
    int xs[MAX_SHIP_LENGTH], ys[MAX_SHIP_LENGTH];
    struct MapObject base = make_ship(OBJECT_BASE_SHIP, 10, 10, ROTATION_UP, 1, 1);
    CHECK_EQ(ship_squares(&base, xs, ys), 5);
    CHECK_EQ(xs[4], 10);
    CHECK_EQ(ys[4], 6); // Head is up.
    struct MapObject boat = make_ship(OBJECT_FAST_BOAT, 3, 3, ROTATION_LEFT, 1, 1);
    CHECK_EQ(ship_squares(&boat, xs, ys), 1);
    struct MapObject sub = make_ship(OBJECT_SUBMARINE, 5, 5, ROTATION_RIGHT, 1, 1);
    CHECK_EQ(ship_segment_at(&sub, 5, 5), 0);
    CHECK_EQ(ship_segment_at(&sub, 7, 5), 2);
    CHECK_EQ(ship_segment_at(&sub, 8, 5), -1);
    struct MapObject not_ship = make_ship(OBJECT_HIT_SQUARE, 1, 1, 0, 1, 0);
    CHECK_EQ(ship_squares(&not_ship, xs, ys), 0);
    struct MapObject bad_rotation = make_ship(OBJECT_SMALL_SHIP, 1, 1, 7, 1, 0);
    CHECK_EQ(ship_squares(&bad_rotation, xs, ys), 0);

    CHECK(!ship_in_map(&base, 50, 50) == 0);
    struct MapObject edge = make_ship(OBJECT_BASE_SHIP, 0, 3, ROTATION_UP, 1, 1);
    CHECK(!ship_in_map(&edge, 50, 50)); // Head would be at y = -1.
    edge.rotation = ROTATION_LEFT;
    CHECK(!ship_in_map(&edge, 50, 50));
    edge.rotation = ROTATION_RIGHT;
    CHECK(ship_in_map(&edge, 50, 50));

    // Damage and sinking.
    CHECK(!ship_is_damaged(&sub));
    sub.info = 2;
    CHECK(ship_is_damaged(&sub));
    CHECK(!ship_is_sunk(&sub));
    CHECK_EQ(ship_healthy_segments(&sub), 2);
    sub.info = 7;
    CHECK(ship_is_sunk(&sub));
}

static void test_zones_and_placement()
{
    printf("Team zones and placement\n");
    int x0, y0, x1, y1;
    CHECK_EQ(team_zone(1, 50, 50, &x0, &y0, &x1, &y1), 0);
    CHECK(x0 == 0 && y0 == 0 && x1 == 19 && y1 == 19);
    CHECK_EQ(team_zone(2, 50, 50, &x0, &y0, &x1, &y1), 0);
    CHECK(x0 == 30 && y0 == 30 && x1 == 49 && y1 == 49);
    CHECK_EQ(team_zone(3, 50, 40, &x0, &y0, &x1, &y1), 0);
    CHECK(x0 == 30 && y0 == 0 && x1 == 49 && y1 == 19);
    CHECK_EQ(team_zone(4, 40, 40, &x0, &y0, &x1, &y1), 0);
    CHECK(x0 == 0 && y0 == 20 && x1 == 19 && y1 == 39);
    CHECK_EQ(team_zone(0, 50, 50, &x0, &y0, &x1, &y1), -1);
    CHECK_EQ(team_zone(5, 50, 50, &x0, &y0, &x1, &y1), -1);

    struct MapObject ships[10];
    size_t count = 0;
    struct MapObject ship = make_ship(OBJECT_BATTLESHIP, 2, 10, ROTATION_UP, 1, 1);
    CHECK_EQ(validate_placement(&ship, ships, count, 50, 50), RULE_OK);
    ships[count++] = ship;
    struct MapObject crossing = make_ship(OBJECT_SMALL_SHIP, 1, 8, ROTATION_RIGHT, 1, 2);
    CHECK_EQ(validate_placement(&crossing, ships, count, 50, 50), RULE_OCCUPIED);
    struct MapObject outside_zone = make_ship(OBJECT_SMALL_SHIP, 19, 5, ROTATION_RIGHT, 1, 2);
    CHECK_EQ(validate_placement(&outside_zone, ships, count, 50, 50), RULE_OUT_OF_ZONE);
    struct MapObject enemy_zone = make_ship(OBJECT_SMALL_SHIP, 40, 40, ROTATION_RIGHT, 1, 2);
    CHECK_EQ(validate_placement(&enemy_zone, ships, count, 50, 50), RULE_OUT_OF_ZONE);
    struct MapObject outside_map = make_ship(OBJECT_SMALL_SHIP, 0, 0, ROTATION_UP, 1, 2);
    CHECK_EQ(validate_placement(&outside_map, ships, count, 50, 50), RULE_OUT_OF_MAP);
    struct MapObject bad_type = make_ship(OBJECT_MISSED_HIT, 5, 5, ROTATION_UP, 1, 2);
    CHECK_EQ(validate_placement(&bad_type, ships, count, 50, 50), RULE_INVALID_SHIP_TYPE);
    struct MapObject bad_rotation = make_ship(OBJECT_SMALL_SHIP, 5, 5, 4, 1, 2);
    CHECK_EQ(validate_placement(&bad_rotation, ships, count, 50, 50), RULE_INVALID_ROTATION);
    // Sunk ships don't block.
    ships[0].info = 0x0F;
    CHECK_EQ(validate_placement(&crossing, ships, count, 50, 50), RULE_OK);

    // Many random placements are always valid and never overlap.
    struct MapObject many[60];
    size_t placed = 0;
    int all_valid = 1;
    while (placed < 60)
    {
        struct MapObject random_ship = make_ship((uint8_t)(placed % 5 + 1), 0, 0, 0, (uint8_t)(placed % 2 + 1), (uint8_t)placed);
        if (random_placement(&random_ship, many, placed, 40, 40, rand) != RULE_OK ||
            validate_placement(&random_ship, many, placed, 40, 40) != RULE_OK)
        {
            all_valid = 0;
        }
        many[placed++] = random_ship;
    }
    CHECK(all_valid);
}

static void test_movement()
{
    printf("Ship movement\n");
    struct MapObject ships[3];
    ships[0] = make_ship(OBJECT_SUBMARINE, 10, 10, ROTATION_RIGHT, 1, 1); // (10..12, 10)
    ships[1] = make_ship(OBJECT_SMALL_SHIP, 14, 9, ROTATION_DOWN, 2, 2);   // (14, 9..10)
    ships[2] = make_ship(OBJECT_BASE_SHIP, 0, 30, ROTATION_UP, 1, 1);
    struct MapObject *sub = &ships[0];

    CHECK_EQ(move_ship(sub, ROTATION_RIGHT, ships, 3, 50, 50), RULE_OK);
    CHECK(sub->x == 11 && sub->y == 10);
    // Head would move to (14, 10) where enemy is.
    CHECK_EQ(move_ship(sub, ROTATION_RIGHT, ships, 3, 50, 50), RULE_OCCUPIED);
    CHECK(sub->x == 11);
    CHECK_EQ(move_ship(sub, ROTATION_LEFT, ships, 3, 50, 50), RULE_CANNOT_TURN_AROUND);
    CHECK_EQ(move_ship(sub, ROTATION_DOWN, ships, 3, 50, 50), RULE_OK);
    CHECK(sub->rotation == ROTATION_DOWN && sub->x == 11 && sub->y == 10);
    CHECK_EQ(move_ship(sub, 9, ships, 3, 50, 50), RULE_INVALID_ROTATION);

    // Base Ship can't move.
    CHECK_EQ(move_ship(&ships[2], ROTATION_UP, ships, 3, 50, 50), RULE_SHIP_CANNOT_MOVE);
    // Damaged ship can't move.
    ships[1].info = 1;
    CHECK_EQ(move_ship(&ships[1], ROTATION_DOWN, ships, 3, 50, 50), RULE_SHIP_DAMAGED);
    // Map edge.
    struct MapObject edge = make_ship(OBJECT_SMALL_SHIP, 0, 1, ROTATION_UP, 1, 1); // (0, 1..0)
    CHECK_EQ(move_ship(&edge, ROTATION_UP, &edge, 1, 50, 50), RULE_OUT_OF_MAP);
    CHECK_EQ(move_ship(&edge, ROTATION_LEFT, &edge, 1, 50, 50), RULE_OUT_OF_MAP);
    CHECK_EQ(move_ship(&edge, ROTATION_RIGHT, &edge, 1, 50, 50), RULE_OK);
    // Fast Boat is one square and can turn any way.
    struct MapObject boat = make_ship(OBJECT_FAST_BOAT, 5, 5, ROTATION_UP, 1, 1);
    CHECK_EQ(move_ship(&boat, ROTATION_DOWN, &boat, 1, 50, 50), RULE_OK);
    CHECK_EQ(move_ship(&boat, ROTATION_DOWN, &boat, 1, 50, 50), RULE_OK);
    CHECK(boat.y == 6);
    // Sunk ship doesn't block.
    struct MapObject wreck_test[2] = {make_ship(OBJECT_FAST_BOAT, 5, 5, ROTATION_RIGHT, 1, 1), make_ship(OBJECT_FAST_BOAT, 6, 5, 0, 2, 2)};
    CHECK_EQ(move_ship(&wreck_test[0], ROTATION_RIGHT, wreck_test, 2, 50, 50), RULE_OCCUPIED);
    wreck_test[1].info = 1;
    CHECK_EQ(move_ship(&wreck_test[0], ROTATION_RIGHT, wreck_test, 2, 50, 50), RULE_OK);
}

static void test_shooting()
{
    printf("Shooting\n");
    struct MapObject base = make_ship(OBJECT_BASE_SHIP, 0, 4, ROTATION_UP, 1, 1);
    CHECK_EQ(validate_shot(&base, 49, 49, 50, 50), RULE_OK); // Infinite range.
    CHECK_EQ(validate_shot(&base, 50, 49, 50, 50), RULE_OUT_OF_MAP);
    CHECK_EQ(validate_shot(&base, -1, 0, 50, 50), RULE_OUT_OF_MAP);

    struct MapObject battleship = make_ship(OBJECT_BATTLESHIP, 10, 10, ROTATION_RIGHT, 1, 1); // (10..13, 10)
    CHECK_EQ(validate_shot(&battleship, 13 + 25, 10, 50, 50), RULE_OK);
    CHECK_EQ(validate_shot(&battleship, 13 + 26, 10, 50, 50), RULE_OUT_OF_RANGE);
    CHECK_EQ(validate_shot(&battleship, 13 + 15, 10 + 20, 50, 50), RULE_OK); // 15^2 + 20^2 = 25^2
    CHECK_EQ(validate_shot(&battleship, 13 + 16, 10 + 20, 50, 50), RULE_OUT_OF_RANGE);

    struct MapObject small = make_ship(OBJECT_SMALL_SHIP, 20, 20, ROTATION_DOWN, 1, 1); // (20, 20..21)
    CHECK_EQ(validate_shot(&small, 20, 31, 50, 50), RULE_OK);
    CHECK_EQ(validate_shot(&small, 20, 32, 50, 50), RULE_OUT_OF_RANGE);

    struct MapObject sub = make_ship(OBJECT_SUBMARINE, 5, 5, ROTATION_DOWN, 1, 1); // (5, 5..7)
    CHECK_EQ(validate_shot(&sub, 5, 49, 50, 50), RULE_OK);
    CHECK_EQ(validate_shot(&sub, 49, 7, 50, 50), RULE_OK);
    CHECK_EQ(validate_shot(&sub, 6, 9, 50, 50), RULE_OUT_OF_RANGE);

    struct MapObject boat = make_ship(OBJECT_FAST_BOAT, 1, 1, 0, 1, 1);
    CHECK_EQ(validate_shot(&boat, 1, 2, 50, 50), RULE_SHIP_CANNOT_SHOOT);
    base.info = 0x1F;
    CHECK_EQ(validate_shot(&base, 10, 10, 50, 50), RULE_SHIP_SUNK);

    struct MapObject ships[2] = {make_ship(OBJECT_SMALL_SHIP, 30, 30, ROTATION_RIGHT, 2, 2), make_ship(OBJECT_FAST_BOAT, 35, 35, 0, 2, 3)};
    int index;
    CHECK_EQ(apply_shot(ships, 2, 32, 30, &index), SHOT_MISS);
    CHECK_EQ(index, -1);
    CHECK_EQ(apply_shot(ships, 2, 31, 30, &index), SHOT_HIT);
    CHECK_EQ(index, 0);
    CHECK_EQ(ships[0].info, 2); // Second segment from tail.
    CHECK_EQ(apply_shot(ships, 2, 31, 30, &index), SHOT_ALREADY_HIT);
    CHECK_EQ(apply_shot(ships, 2, 30, 30, &index), SHOT_SUNK);
    // Sunk ship is not hit again.
    CHECK_EQ(apply_shot(ships, 2, 30, 30, &index), SHOT_MISS);
    CHECK_EQ(apply_shot(ships, 2, 35, 35, &index), SHOT_SUNK);
    CHECK_EQ(index, 1);
    CHECK_EQ(count_alive_ships(ships, 2, 2), 0);
    CHECK(!team_ship_at(ships, 2, 2, 30, 30));
}

static void test_visibility()
{
    printf("Fog of war\n");
    struct MapObject ships[4];
    ships[0] = make_ship(OBJECT_BATTLESHIP, 10, 10, ROTATION_RIGHT, 1, 1); // Vision 10
    ships[1] = make_ship(OBJECT_SMALL_SHIP, 20, 16, ROTATION_DOWN, 2, 2);  // Distance from (13,10) to (20,16) = sqrt(85) < 10
    ships[2] = make_ship(OBJECT_SMALL_SHIP, 40, 40, ROTATION_DOWN, 2, 3);  // Far away
    ships[3] = make_ship(OBJECT_FAST_BOAT, 25, 25, 0, 1, 1);               // Vision 15
    CHECK(team_sees_object(1, &ships[1], ships, 4));
    CHECK(!team_sees_object(1, &ships[2], ships, 4)); // Fast Boat: (25,25)-(40,40) = sqrt(450) > 15
    CHECK(team_sees_object(2, &ships[0], ships, 4));
    CHECK(team_sees_object(0, &ships[2], ships, 4)); // Observer sees everything.
    CHECK(team_sees_object(2, &ships[2], ships, 4)); // Own ships.
    ships[3].x = 30;
    ships[3].y = 35; // (30,35)-(40,40) = sqrt(125) < 15
    CHECK(team_sees_object(1, &ships[2], ships, 4));
    ships[3].x = 10;
    ships[3].y = 40;
    CHECK(!team_sees_object(1, &ships[2], ships, 4));
    // Sunk ships are known to everyone.
    ships[2].info = 3;
    CHECK(team_sees_object(1, &ships[2], ships, 4));
    // Sunk ships don't see.
    ships[0].info = 0x0F;
    CHECK(!team_sees_square(1, 13, 10, ships, 4));
    CHECK_EQ(count_alive_ships(ships, 4, 1), 1);
    CHECK_EQ(count_healthy_segments(ships, 4, 2), 2);
}

// ---------------------------------------------------------------------------
// Config
// ---------------------------------------------------------------------------

static void test_config()
{
    printf("Config\n");
    set_log_level(LOG_LEVEL_ERROR + 1); // Errors are expected here.
    struct ServerConfig config;
    config_defaults(&config);
    CHECK_EQ(config.port, 12345);
    char *good[] = {"server", "--port", "4000", "--teams", "4", "--bind", "127.0.0.1"};
    CHECK_EQ(config_parse_args(&config, 7, good, "test"), 0);
    CHECK_EQ(config.port, 4000);
    CHECK_EQ(config.team_count, 4);
    CHECK(strcmp(config.bind_address, "127.0.0.1") == 0);
    char *bad_value[] = {"server", "--teams", "5"};
    CHECK_EQ(config_parse_args(&config, 3, bad_value, "test"), -1);
    char *not_number[] = {"server", "--port", "12ab"};
    CHECK_EQ(config_parse_args(&config, 3, not_number, "test"), -1);
    char *unknown[] = {"server", "--colour", "red"};
    CHECK_EQ(config_parse_args(&config, 3, unknown, "test"), -1);
    char *missing[] = {"server", "--port"};
    CHECK_EQ(config_parse_args(&config, 2, missing, "test"), -1);

    char path[] = "/tmp/battleships_config_XXXXXX";
    int file = mkstemp(path);
    const char *text = "# Comment\nmap-width = 60\n\n  max-turns=99  \nport = 5000\n";
    CHECK_EQ(write(file, text, strlen(text)), (ssize_t)strlen(text));
    close(file);
    char *with_file[] = {"server", "--port", "6000", "--config", path};
    config_defaults(&config);
    CHECK_EQ(config_parse_args(&config, 5, with_file, "test"), 0);
    CHECK_EQ(config.map_width, 60);
    CHECK_EQ(config.max_turns, 99);
    CHECK_EQ(config.port, 6000); // Command line wins.
    unlink(path);

    config_defaults(&config);
    config.min_players = 8;
    config.max_players = 4;
    CHECK_EQ(config_validate(&config), -1);
    set_log_level(LOG_LEVEL_INFO);
}

int main()
{
    srand(1);
    test_simple_packets();
    test_state_packet();
    test_connection();
    test_ship_geometry();
    test_zones_and_placement();
    test_movement();
    test_shooting();
    test_visibility();
    test_config();
    printf("\n%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
