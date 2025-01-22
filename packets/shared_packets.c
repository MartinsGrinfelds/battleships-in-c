#include "shared_packets.h"
#include <stdint.h> // uint definitions
#include <stdlib.h> // exit(), size_t, malloc
#include <stdio.h> // printf
#include "../graphical/text_formatter.h" // print_failure()

// IMPORTANT!!! SERIALIZATION IS NOT NEEDED IF PACKET CONTAINS PURE VALUES.
// Packet serialization/deserialization helps to put information together in one chunk.
// Without this deserialized packets contain information about fields.
// Fields can contain address to another memory and normally sending the packet we would not send data but only address.
// It is required to read multiple address contents into one chunk of data.
// To make this working you have to deserialize in same order as you serialized the content.

//TODO: Fix potencial issue with filler_pointer + sizeof. This is wrong and works because sizeof returns 1

char *hello_packet_serialization(struct HelloPacket *packet, size_t *final_size)
{
    *final_size = sizeof(packet->name_length) + packet->name_length;
    // Call free(address) for this one or enjoy memory leak 😍
    char* serialized_packet = malloc(*final_size);
    char *filler_pointer = serialized_packet;
    *(uint8_t *)filler_pointer = packet->name_length;
    filler_pointer += sizeof(uint8_t);
    char *name_pointer = packet->name;
    while ((size_t)filler_pointer - (size_t)serialized_packet < *final_size)
    {
        *filler_pointer++ = *name_pointer++;
    }
    return serialized_packet;
}

void hello_packet_deserialization(char *serialized_packet, struct HelloPacket *deserialized_packet)
{
    char *read_pointer = serialized_packet;
    deserialized_packet->name_length = *(uint8_t *)read_pointer++;
    
    deserialized_packet->name = malloc(deserialized_packet->name_length + 1); // +1 is for \0 character
    char *write_pointer = deserialized_packet->name;
    char* name_start_pointer = read_pointer;
    // read_pointer points at first name character.
    // By subtracting name start pointer we can know processed name length.
    while (read_pointer - name_start_pointer < deserialized_packet->name_length)
    {
        *write_pointer++ = *read_pointer++;
    }
    *write_pointer = '\0'; // Fill last name character with \0 or risk hoping it was 0
    return;
}

char *ack_packet_serialization(struct AckPacket *packet, size_t *final_size)
{
    *final_size = sizeof(packet->team_id) + sizeof(packet->player_id) + sizeof(packet->message_length) + packet->message_length;
    // Call free(address) for this one or enjoy memory leak 😍
    char* serialized_packet = malloc(*final_size);
    char *filler_pointer = serialized_packet;
    // Player ID
    *(uint8_t *)filler_pointer = packet->player_id;
    filler_pointer += sizeof(uint8_t);
    // Team ID
    *(uint8_t *)filler_pointer = packet->team_id;
    filler_pointer += sizeof(uint8_t);
    // Message length
    *(uint8_t *)filler_pointer = packet->message_length;
    filler_pointer += sizeof(uint8_t);
    // Message
    char *message_pointer = packet->message;
    while ((size_t)filler_pointer - (size_t)serialized_packet < *final_size)
    {
        *filler_pointer++ = *message_pointer++;
    }
    return serialized_packet;
}



void ack_packet_deserialization(char *serialized_packet, struct AckPacket *deserialized_packet)
{
    char *read_pointer = serialized_packet;
    // Player ID
    deserialized_packet->player_id = *(uint8_t *)read_pointer++;
    // Team ID
    deserialized_packet->team_id = *(uint8_t *)read_pointer++;
    // Message length
    deserialized_packet->message_length = *(uint8_t *)read_pointer++;
    char* message_start_pointer = read_pointer;
    // Message
    deserialized_packet->message = malloc(deserialized_packet->message_length + 1); // +1 is for \0 character
    char *write_pointer = deserialized_packet->message;
    // read_pointer points at first message character.
    // By subtracting message start pointer we can know processed message length.
    while (read_pointer - message_start_pointer < deserialized_packet->message_length)
    {
        *write_pointer++ = *read_pointer++;
    }
    *write_pointer = '\0'; // Fill last message character with \0 or risk hoping it was 0
    return;
}

// char *player_packet_serialization(struct Player *packet, size_t *final_size)
// {
//     *final_size = sizeof(packet->socket_nr) + sizeof(packet->id) + sizeof(packet->team_id)+ sizeof(packet->message_length) + packet->message_length;
//     // int socket_nr;
//     // uint8_t id;
//     // uint8_t team_id;
//     // uint8_t status;
//     // char username[MAX_USERNAME]; NO NEED TO SERIALIZE IF FIXED SIZE
// }

// void player_packet_deserialization(char *serialized_packet, Player *deserialized_packet)
// {
// }

char *state_packet_serialization(struct StatePacket *packet, size_t *final_size)
{
    *final_size = sizeof(packet->map_width) + sizeof(packet->map_height) + sizeof(packet->status) + sizeof(packet->object_count) + sizeof(packet->player_count);
    *final_size += (packet->object_count * sizeof(struct MapObject)) + (packet->player_count * sizeof(struct Player));
    // Call free(address) for this one or enjoy memory leak 😍
    char* serialized_packet = malloc(*final_size);
    char *filler_pointer = serialized_packet;
    // uint8_t map_width;
    *(uint8_t *)filler_pointer = packet->map_width;
    filler_pointer += sizeof(uint8_t);
    // uint8_t map_height;
    *(uint8_t *)filler_pointer = packet->map_height;
    filler_pointer += sizeof(uint8_t);
    // uint8_t status;
    *(uint8_t *)filler_pointer = packet->status;
    filler_pointer += sizeof(uint8_t);
    // uint8_t object_count;
    *(uint8_t *)filler_pointer = packet->object_count;
    filler_pointer += sizeof(uint8_t);
    // struct MapObject *map_objects;
    struct MapObject *object_filler_pointer = (struct MapObject *)filler_pointer;
    struct MapObject *object_pointer = packet->map_objects;
    size_t counter = 0;
    while (counter++ < packet->object_count)
    {
        *object_filler_pointer++ = *object_pointer++;
    }
    filler_pointer = (char *)object_filler_pointer;
    // uint8_t player_count;
    *(uint8_t *)filler_pointer = packet->player_count;
    filler_pointer += sizeof(uint8_t);
    // struct Player *players;
    struct Player *player_filler_pointer = (struct Player *)filler_pointer;
    struct Player *player_pointer = packet->players;
    counter = 0;
    while (counter++ < packet->player_count)
    {
        *player_filler_pointer++ = *player_pointer++;
    }
    return serialized_packet;
}

void state_packet_deserialization(char *serialized_packet, struct StatePacket *deserialized_packet)
{
    // Checks of already initialized memory.
    if (deserialized_packet->map_objects)
    {
        print_warning("WARNING: Deserialization map objects already initialized. Freeing.\n");
        free(deserialized_packet->map_objects);
        deserialized_packet->map_objects = NULL;
    }
    if (deserialized_packet->players)
    {
        print_warning("WARNING: Deserialization players already initialized. Freeing.\n");
        free(deserialized_packet->players);
        deserialized_packet->players = NULL;
    }

    char *read_pointer = serialized_packet;
    // uint8_t map_width;
    deserialized_packet->map_width = *(uint8_t *)read_pointer++;
    // uint8_t map_height;
    deserialized_packet->map_height = *(uint8_t *)read_pointer++;
    // uint8_t status;
    deserialized_packet->status = *(uint8_t *)read_pointer++;
    // uint8_t object_count;
    deserialized_packet->object_count = *(uint8_t *)read_pointer++;
    // struct MapObject *map_objects;
    if (deserialized_packet->object_count)
    {
        deserialized_packet->map_objects = calloc(deserialized_packet->object_count, sizeof(struct MapObject));
        struct MapObject* object_start_pointer = (void *)read_pointer;
        struct MapObject *object_read_pointer = object_start_pointer;
        struct MapObject *object_write_pointer = deserialized_packet->map_objects;
        while (((struct MapObject *)object_read_pointer - (struct MapObject *)object_start_pointer) < deserialized_packet->object_count)
        {
            *object_write_pointer++ = *object_read_pointer++;
        }
        read_pointer = (void *)object_read_pointer;
    }
    // uint8_t player_count;
    deserialized_packet->player_count = *(uint8_t *)read_pointer++;
    // struct Player *players;
    if (deserialized_packet->player_count)
    {
        deserialized_packet->players = calloc(deserialized_packet->player_count, sizeof(struct Player));
        struct Player *player_start_pointer = (struct Player *)read_pointer;
        struct Player *player_read_pointer = player_start_pointer;
        struct Player *player_write_pointer = deserialized_packet->players;
        while (((struct Player *)player_read_pointer - (struct Player *)player_start_pointer) < deserialized_packet->player_count)
        {
            *player_write_pointer++ = *player_read_pointer++;
        }
    }
}

char *message_packet_serialization(struct MessagePacket *packet, size_t *final_size)
{
    *final_size = sizeof(packet->message_type) + sizeof(packet->sender_id) + sizeof(packet->receiver_id) + sizeof(packet->message_length) + packet->message_length;
    // Call free(address) for this one or enjoy memory leak 😍
    char* serialized_packet = malloc(*final_size);
    char *filler_pointer = serialized_packet;
    // uint8_t message_type;
    *(uint8_t *)filler_pointer = packet->message_type;
    filler_pointer += sizeof(uint8_t);
    // uint8_t sender_id;
    *(uint8_t *)filler_pointer = packet->sender_id;
    filler_pointer += sizeof(uint8_t);
    // uint8_t receiver_id;
    *(uint8_t *)filler_pointer = packet->receiver_id;
    filler_pointer += sizeof(uint8_t);
    // uint8_t message_length;
    *(uint8_t *)filler_pointer = packet->message_length;
    filler_pointer += sizeof(uint8_t);
    // char *message;
    char *message_pointer = packet->message;
    while ((size_t)filler_pointer - (size_t)serialized_packet < *final_size)
    {
        *filler_pointer++ = *message_pointer++;
    }
    return serialized_packet;
}

void message_packet_deserialization(char *serialized_packet, struct MessagePacket *deserialized_packet)
{
    char *read_pointer = serialized_packet;
    // uint8_t message_type;
    deserialized_packet->message_type = *(uint8_t *)read_pointer++;
    // uint8_t sender_id;
    deserialized_packet->sender_id = *(uint8_t *)read_pointer++;
    // uint8_t receiver_id;
    deserialized_packet->receiver_id = *(uint8_t *)read_pointer++;
    // uint8_t message_length;
    deserialized_packet->message_length = *(uint8_t *)read_pointer++;
    // char *message;
    deserialized_packet->message = malloc(deserialized_packet->message_length + 1); // +1 is for \0 character
    char *write_pointer = deserialized_packet->message;
    char* message_start_pointer = read_pointer;
    // read_pointer points at first message character.
    // By subtracting message start pointer we can know processed name length.
    while (read_pointer - message_start_pointer < deserialized_packet->message_length)
    {
        *write_pointer++ = *read_pointer++;
    }
    *write_pointer = '\0'; // Fill last name character with \0 or risk hoping it was 0
    return;
}

char *i_place_packet_serialization(struct IPlacePacket *packet, size_t *final_size)
{
    *final_size = sizeof(struct MapObject);

    char* serialized_packet = malloc(*final_size);
    char *filler_pointer = serialized_packet;
    // struct MapObject object;
    *(struct MapObject *)filler_pointer = packet->object;
    filler_pointer += sizeof(struct MapObject);

    return serialized_packet;
}

void i_place_packet_deserialization(char *serialized_packet, struct IPlacePacket *deserialized_packet)
{
    char *read_pointer = serialized_packet;

    // struct MapObject object;
    deserialized_packet->object = *(struct MapObject *)read_pointer;
    read_pointer += sizeof(struct MapObject);
    return;
}
