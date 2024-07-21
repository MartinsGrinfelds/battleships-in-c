#include "shared_packets.h"
#include <stdint.h> // uint definitions
#include <stdlib.h> // exit(), size_t, malloc
#include <stdio.h> // printf

char *hello_packet_serialization(struct HelloPacket *packet, size_t *final_size)
{
    *final_size = sizeof(packet->name_length) + packet->name_length;
    // Call free(address) for this one or enjoy memory leak 😍
    char* decompressed_packet = malloc(*final_size);
    char *filler_pointer = decompressed_packet;
    *(uint8_t *)filler_pointer = packet->name_length;
    filler_pointer += sizeof(uint8_t);
    char *name_pointer = packet->name;
    while ((size_t)filler_pointer - (size_t)decompressed_packet < *final_size)
    {
        *filler_pointer++ = *name_pointer++;
    }
    return decompressed_packet;
}

void hello_packet_deserialization(char *serialized_packet, struct HelloPacket *deserialized_packet)
{
    char *read_pointer = serialized_packet;
    deserialized_packet->name_length = *(uint8_t *)read_pointer;
    
    deserialized_packet->name = malloc(deserialized_packet->name_length + 1); // +1 is \0
    read_pointer++;
    char *write_pointer = deserialized_packet->name;
    printf("Deserialize: ");
    while (read_pointer - serialized_packet <= deserialized_packet->name_length)
    {
        printf("%c", *read_pointer);
        *write_pointer++ = *read_pointer++;
    }
    printf("\n");
    write_pointer = '\0'; // For possibility to use printf()
    return;
}
// TODO: Add other packet serialization/deserialization
