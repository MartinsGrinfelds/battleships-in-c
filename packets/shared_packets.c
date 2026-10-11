#include "shared_packets.h"
#include <stdint.h> // uint definitions
#include <stdlib.h> // exit(), size_t, malloc
#include <string.h> // memcpy(), strlen()

// IMPORTANT!!! Packet serialization/deserialization helps to put information together in one chunk.
// Without this deserialized packets contain information about fields.
// Fields can contain address to another memory and normally sending the packet we would not send data but only address.
// It is required to read multiple address contents into one chunk of data.
// To make this working you have to deserialize in same order as you serialized the content.
// Structs are never copied as raw memory because compilers may add padding and machines differ in byte order.

#define SERIALIZED_MAP_OBJECT_SIZE 7
#define SERIALIZED_PLAYER_SIZE (3 + MAX_USERNAME + 1)

/// @brief Helper to write values one after another into serialized packet.
struct Writer
{
    char *start;
    char *position;
};

/// @brief Helper to read values one after another and never read outside of received content.
struct Reader
{
    char *position;
    size_t left;
    int failed;
};

static char *writer_start(struct Writer *writer, size_t size)
{
    // Call free(address) for this one or enjoy memory leak 😍
    // +1 so that malloc(0) is never called.
    writer->start = malloc(size + 1);
    writer->position = writer->start;
    return writer->start;
}

static void write_u8(struct Writer *writer, uint8_t value)
{
    *(uint8_t *)writer->position = value;
    writer->position += sizeof(uint8_t);
}

static void write_u16(struct Writer *writer, uint16_t value)
{
    // Network byte order (Big-Endian).
    write_u8(writer, (uint8_t)(value >> 8));
    write_u8(writer, (uint8_t)(value & 0xFF));
}

static void write_bytes(struct Writer *writer, const char *bytes, size_t length)
{
    if (length)
    {
        memcpy(writer->position, bytes, length);
        writer->position += length;
    }
}

static uint8_t read_u8(struct Reader *reader)
{
    if (reader->left < 1)
    {
        reader->failed = 1;
        return 0;
    }
    uint8_t value = *(uint8_t *)reader->position;
    reader->position++;
    reader->left--;
    return value;
}

static uint16_t read_u16(struct Reader *reader)
{
    uint16_t value = read_u8(reader);
    value = (value << 8) | read_u8(reader);
    return value;
}

/// @brief Reads string of given length and returns it as \0 terminated string created by malloc.
static char *read_string(struct Reader *reader, size_t length)
{
    if (reader->left < length)
    {
        reader->failed = 1;
        return NULL;
    }
    char *string = malloc(length + 1); // +1 is for \0 character
    memcpy(string, reader->position, length);
    string[length] = '\0'; // Fill last character with \0 or risk hoping it was 0
    reader->position += length;
    reader->left -= length;
    return string;
}

static void reader_start(struct Reader *reader, char *serialized_packet, size_t size)
{
    reader->position = serialized_packet;
    reader->left = serialized_packet ? size : 0;
    reader->failed = 0;
}

static uint8_t safe_string_length(char *string, uint8_t length)
{
    // Never trust length if string is missing.
    return string ? length : 0;
}

char *hello_packet_serialization(struct HelloPacket *packet, size_t *final_size)
{
    uint8_t name_length = safe_string_length(packet->name, packet->name_length);
    *final_size = sizeof(packet->name_length) + name_length;
    struct Writer writer;
    writer_start(&writer, *final_size);
    write_u8(&writer, name_length);
    write_bytes(&writer, packet->name, name_length);
    return writer.start;
}

int hello_packet_deserialization(char *serialized_packet, size_t size, struct HelloPacket *deserialized_packet)
{
    struct Reader reader;
    reader_start(&reader, serialized_packet, size);
    deserialized_packet->name_length = read_u8(&reader);
    deserialized_packet->name = read_string(&reader, deserialized_packet->name_length);
    return reader.failed ? -1 : 0;
}

char *ack_packet_serialization(struct AckPacket *packet, size_t *final_size)
{
    uint8_t message_length = safe_string_length(packet->message, packet->message_length);
    *final_size = sizeof(packet->team_id) + sizeof(packet->player_id) + sizeof(packet->message_length) + message_length;
    struct Writer writer;
    writer_start(&writer, *final_size);
    write_u8(&writer, packet->player_id);
    write_u8(&writer, packet->team_id);
    write_u8(&writer, message_length);
    write_bytes(&writer, packet->message, message_length);
    return writer.start;
}

int ack_packet_deserialization(char *serialized_packet, size_t size, struct AckPacket *deserialized_packet)
{
    struct Reader reader;
    reader_start(&reader, serialized_packet, size);
    deserialized_packet->player_id = read_u8(&reader);
    deserialized_packet->team_id = read_u8(&reader);
    deserialized_packet->message_length = read_u8(&reader);
    deserialized_packet->message = read_string(&reader, deserialized_packet->message_length);
    return reader.failed ? -1 : 0;
}

char *state_packet_serialization(struct StatePacket *packet, size_t *final_size)
{
    uint8_t object_count = packet->map_objects ? packet->object_count : 0;
    uint8_t player_count = packet->players ? packet->player_count : 0;
    *final_size = sizeof(packet->map_width) + sizeof(packet->map_height) + sizeof(packet->status) + sizeof(packet->object_count) + sizeof(packet->player_count);
    *final_size += (object_count * SERIALIZED_MAP_OBJECT_SIZE) + (player_count * SERIALIZED_PLAYER_SIZE);
    struct Writer writer;
    writer_start(&writer, *final_size);
    write_u8(&writer, packet->map_width);
    write_u8(&writer, packet->map_height);
    write_u8(&writer, packet->status);
    write_u8(&writer, object_count);
    size_t counter = 0;
    while (counter < object_count)
    {
        struct MapObject *object = &packet->map_objects[counter++];
        write_u8(&writer, object->object_type);
        write_u8(&writer, object->x);
        write_u8(&writer, object->y);
        write_u8(&writer, object->rotation);
        write_u8(&writer, object->team_id);
        write_u8(&writer, object->info);
        write_u8(&writer, object->owner_id);
    }
    write_u8(&writer, player_count);
    counter = 0;
    while (counter < player_count)
    {
        struct Player *player = &packet->players[counter++];
        char username[MAX_USERNAME + 1] = {0};
        strncpy(username, player->username, MAX_USERNAME);
        write_u8(&writer, player->id);
        write_u8(&writer, player->team_id);
        write_u8(&writer, player->status);
        write_bytes(&writer, username, MAX_USERNAME + 1);
    }
    return writer.start;
}

void free_state_packet(struct StatePacket *packet)
{
    if (packet->map_objects)
    {
        free(packet->map_objects);
        packet->map_objects = NULL;
    }
    if (packet->players)
    {
        free(packet->players);
        packet->players = NULL;
    }
    packet->object_count = 0;
    packet->player_count = 0;
}

int state_packet_deserialization(char *serialized_packet, size_t size, struct StatePacket *deserialized_packet)
{
    // Info already present. Cleaning!
    free_state_packet(deserialized_packet);
    struct Reader reader;
    reader_start(&reader, serialized_packet, size);
    deserialized_packet->map_width = read_u8(&reader);
    deserialized_packet->map_height = read_u8(&reader);
    deserialized_packet->status = read_u8(&reader);
    uint8_t object_count = read_u8(&reader);
    if (reader.failed || reader.left < (size_t)object_count * SERIALIZED_MAP_OBJECT_SIZE)
    {
        return -1;
    }
    if (object_count)
    {
        deserialized_packet->map_objects = calloc(object_count, sizeof(struct MapObject));
    }
    size_t counter = 0;
    while (counter < object_count)
    {
        struct MapObject *object = &deserialized_packet->map_objects[counter++];
        object->object_type = read_u8(&reader);
        object->x = read_u8(&reader);
        object->y = read_u8(&reader);
        object->rotation = read_u8(&reader);
        object->team_id = read_u8(&reader);
        object->info = read_u8(&reader);
        object->owner_id = read_u8(&reader);
    }
    deserialized_packet->object_count = object_count;
    uint8_t player_count = read_u8(&reader);
    if (reader.failed || reader.left < (size_t)player_count * SERIALIZED_PLAYER_SIZE)
    {
        free_state_packet(deserialized_packet);
        return -1;
    }
    if (player_count)
    {
        deserialized_packet->players = calloc(player_count, sizeof(struct Player));
    }
    counter = 0;
    while (counter < player_count)
    {
        struct Player *player = &deserialized_packet->players[counter++];
        player->socket_nr = -1;
        player->id = read_u8(&reader);
        player->team_id = read_u8(&reader);
        player->status = read_u8(&reader);
        memcpy(player->username, reader.position, MAX_USERNAME + 1);
        player->username[MAX_USERNAME] = '\0'; // Never trust the network.
        reader.position += MAX_USERNAME + 1;
        reader.left -= MAX_USERNAME + 1;
    }
    deserialized_packet->player_count = player_count;
    return 0;
}

char *message_packet_serialization(struct MessagePacket *packet, size_t *final_size)
{
    uint8_t message_length = safe_string_length(packet->message, packet->message_length);
    *final_size = sizeof(packet->message_type) + sizeof(packet->sender_id) + sizeof(packet->receiver_id) + sizeof(packet->message_length) + message_length;
    struct Writer writer;
    writer_start(&writer, *final_size);
    write_u8(&writer, packet->message_type);
    write_u8(&writer, packet->sender_id);
    write_u8(&writer, packet->receiver_id);
    write_u8(&writer, message_length);
    write_bytes(&writer, packet->message, message_length);
    return writer.start;
}

int message_packet_deserialization(char *serialized_packet, size_t size, struct MessagePacket *deserialized_packet)
{
    struct Reader reader;
    reader_start(&reader, serialized_packet, size);
    deserialized_packet->message_type = read_u8(&reader);
    deserialized_packet->sender_id = read_u8(&reader);
    deserialized_packet->receiver_id = read_u8(&reader);
    deserialized_packet->message_length = read_u8(&reader);
    deserialized_packet->message = read_string(&reader, deserialized_packet->message_length);
    return reader.failed ? -1 : 0;
}

char *you_place_packet_serialization(struct YouPlacePacket *packet, size_t *final_size)
{
    *final_size = sizeof(packet->player_id) + sizeof(packet->object_type);
    struct Writer writer;
    writer_start(&writer, *final_size);
    write_u8(&writer, packet->player_id);
    write_u8(&writer, packet->object_type);
    return writer.start;
}

int you_place_packet_deserialization(char *serialized_packet, size_t size, struct YouPlacePacket *deserialized_packet)
{
    struct Reader reader;
    reader_start(&reader, serialized_packet, size);
    deserialized_packet->player_id = read_u8(&reader);
    deserialized_packet->object_type = read_u8(&reader);
    return reader.failed ? -1 : 0;
}

char *i_place_packet_serialization(struct IPlacePacket *packet, size_t *final_size)
{
    *final_size = SERIALIZED_MAP_OBJECT_SIZE;
    struct Writer writer;
    writer_start(&writer, *final_size);
    write_u8(&writer, packet->object.object_type);
    write_u8(&writer, packet->object.x);
    write_u8(&writer, packet->object.y);
    write_u8(&writer, packet->object.rotation);
    write_u8(&writer, packet->object.team_id);
    write_u8(&writer, packet->object.info);
    write_u8(&writer, packet->object.owner_id);
    return writer.start;
}

int i_place_packet_deserialization(char *serialized_packet, size_t size, struct IPlacePacket *deserialized_packet)
{
    struct Reader reader;
    reader_start(&reader, serialized_packet, size);
    deserialized_packet->object.object_type = read_u8(&reader);
    deserialized_packet->object.x = read_u8(&reader);
    deserialized_packet->object.y = read_u8(&reader);
    deserialized_packet->object.rotation = read_u8(&reader);
    deserialized_packet->object.team_id = read_u8(&reader);
    deserialized_packet->object.info = read_u8(&reader);
    deserialized_packet->object.owner_id = read_u8(&reader);
    return reader.failed ? -1 : 0;
}

char *you_go_packet_serialization(struct YouGoPacket *packet, size_t *final_size)
{
    *final_size = 7 + SHIP_TYPE_COUNT;
    struct Writer writer;
    writer_start(&writer, *final_size);
    write_u8(&writer, packet->player_id);
    write_u16(&writer, packet->turn_number);
    write_u16(&writer, packet->max_turns);
    write_u8(&writer, packet->shots_left);
    write_u8(&writer, packet->seconds_left);
    write_bytes(&writer, (char *)packet->moves_left, SHIP_TYPE_COUNT);
    return writer.start;
}

int you_go_packet_deserialization(char *serialized_packet, size_t size, struct YouGoPacket *deserialized_packet)
{
    struct Reader reader;
    reader_start(&reader, serialized_packet, size);
    deserialized_packet->player_id = read_u8(&reader);
    deserialized_packet->turn_number = read_u16(&reader);
    deserialized_packet->max_turns = read_u16(&reader);
    deserialized_packet->shots_left = read_u8(&reader);
    deserialized_packet->seconds_left = read_u8(&reader);
    size_t i = 0;
    while (i < SHIP_TYPE_COUNT)
    {
        deserialized_packet->moves_left[i++] = read_u8(&reader);
    }
    return reader.failed ? -1 : 0;
}

char *i_go_packet_serialization(struct IGoPacket *packet, size_t *final_size)
{
    *final_size = 6;
    struct Writer writer;
    writer_start(&writer, *final_size);
    write_u8(&writer, packet->player_id);
    write_u8(&writer, packet->action);
    write_u8(&writer, packet->x);
    write_u8(&writer, packet->y);
    write_u8(&writer, packet->object_type);
    write_u8(&writer, packet->direction);
    return writer.start;
}

int i_go_packet_deserialization(char *serialized_packet, size_t size, struct IGoPacket *deserialized_packet)
{
    struct Reader reader;
    reader_start(&reader, serialized_packet, size);
    deserialized_packet->player_id = read_u8(&reader);
    deserialized_packet->action = read_u8(&reader);
    deserialized_packet->x = read_u8(&reader);
    deserialized_packet->y = read_u8(&reader);
    deserialized_packet->object_type = read_u8(&reader);
    deserialized_packet->direction = read_u8(&reader);
    return reader.failed ? -1 : 0;
}

char *end_game_packet_serialization(struct EndGamePacket *packet, size_t *final_size)
{
    *final_size = 2;
    struct Writer writer;
    writer_start(&writer, *final_size);
    write_u8(&writer, packet->winner_id);
    write_u8(&writer, packet->winner_team_id);
    return writer.start;
}

int end_game_packet_deserialization(char *serialized_packet, size_t size, struct EndGamePacket *deserialized_packet)
{
    struct Reader reader;
    reader_start(&reader, serialized_packet, size);
    deserialized_packet->winner_id = read_u8(&reader);
    deserialized_packet->winner_team_id = read_u8(&reader);
    return reader.failed ? -1 : 0;
}
