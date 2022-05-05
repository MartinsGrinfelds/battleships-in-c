// ********************* includes *****************************
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
// ************************

#ifndef PACKET_UTILS
#define PACKET_UTILS "packet_utils.h"

#define MAX_PACKET_SIZE 2060
#define DOUBLE_OUT MAX_PACKET_SIZE*2

char *encode(char *info, uint8_t size);

void* decode(char *info);

void print_smth(char *info);

void print_bytes(void *packet, int count);

unsigned char printable_char(unsigned char c);

void print_bytes(void *packet, int count);

char *from_hex_to_dec(char *info);

char *from_dec_to_hex(char *info);

char *get_checksum(int packet);

#endif /*comment */