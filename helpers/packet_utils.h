// ********************* includes *****************************
#pragma once
#include <ctype.h>
#include <stdint.h>
#include "packet_types.h"
#include <stddef.h>
// ************************

#define MAX_PACKET_SIZE 2060
#define DOUBLE_OUT MAX_PACKET_SIZE*2

char *encode(char *info, size_t size, size_t* encoded_length);

char* decode(char *info, size_t* decoded_length);

void print_smth(char *info);

void print_bytes(void *packet, int count);

unsigned char printable_char(unsigned char c);

void print_bytes(void *packet, int count);

char *from_hex_to_dec(char *info);

char *from_dec_to_hex(char *info);

void process_incoming_packet(int socket, int type, int id);

uint8_t get_checksum(struct GENERIC_PACKET packet);