#include "shared_packets.h"
#include <stdint.h> // uint definitions
#include <stdlib.h> // exit(), size_t, malloc
#include <stdio.h> // printf

// IMPORTANT!!! SERIALIZATION IS NOT NEEDED IF PACKET CONTAINS PURE VALUES.
// Packet serialization/deserialization helps to put information together in one chunk.
// Without this deserialized packets contain information about fields.
// Fields can contain address to another memory and normally sending the packet we would not send data but only address.
// It is required to read multiple address contents into one chunk of data.
// To make this working you have to deserialize in same order as you serialized the content.

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

// TODO: Add other packet serialization/deserialization
