// ********************* includes *****************************
#include "packet_utils.h"
#include <stdio.h>
#include <unistd.h> // read
#include <stdlib.h> // size_t
#include <string.h>
#include <sys/socket.h>
#include "../Server/server_functions.h"
// ************************

char *encode(char *info, size_t size, size_t *encoded_length)
{
  size_t j = 0;
  char *out = malloc(sizeof(char) * DOUBLE_OUT);
  memset(out, 0, DOUBLE_OUT);
  for (size_t i = 0; i < size; i++)
  {
    if (info[i] == '\0')
    {
      out[j++] = '\1';
      out[j++] = '\1';
    }
    else if (info[i] == '\1')
    {
      out[j++] = '\1';
      out[j++] = '\2';
    }
    else
    {
      out[j++] = info[i];
    }
  }
  *encoded_length = j;
  out[++j] = '\0';
  return out;
}

char *decode(char *info, size_t *decoded_length)
{
  size_t j = 0;
  char *out = malloc(sizeof(char) * MAX_PACKET_SIZE);
  for (size_t i = 0; i < DOUBLE_OUT; i++)
  {
    if (info[i] == '\1')
    {
      i++;

      if (info[i] == '\1')
      {
        out[j++] = '\0';
        continue;
      }
      else if (info[i] == '\2')
      {
        out[j++] = '\1';
        continue;
      }
      printf("ERROR! PACKET DECODING FAILURE!!!\n");
      // print_bytes(info, 253);
      exit(1);
      // Error not encoded data passed!!!
    }
    else if (info[i] == '\0')
    {
      break;
    }
    else
    {
      out[j++] = info[i];
    }
  }
  *decoded_length = j;
  return out;
}

void print_smth(char *info)
{ // Ir jau pieejamas divas funkcijas šim.
}

void print_bytes(void *packet, int count);

unsigned char printable_char(unsigned char c)
{
  if (isprint(c) != 0)
    return c;
  return ' ';
}

void print_bytes(void *packet, int count)
{
  int i;
  unsigned char *p = (unsigned char *)packet;
  printf("Printing %d bytes...\n", count);
  printf("[NPK] [C] [HEX] [DEC] [BINARY]\n");
  printf("==============================\n");
  for (i = 0; i < count; i++)
  {
    printf(" %3d | %c |  %02X | %3d | %c%c%c%c%c%c%c%c\n", i, printable_char(p[i]), (unsigned char)p[i], (unsigned char)p[i],
           p[i] & 0x80 ? '1' : '0',
           p[i] & 0x40 ? '1' : '0',
           p[i] & 0x20 ? '1' : '0',
           p[i] & 0x10 ? '1' : '0',
           p[i] & 0x08 ? '1' : '0',
           p[i] & 0x04 ? '1' : '0',
           p[i] & 0x02 ? '1' : '0',
           p[i] & 0x01 ? '1' : '0');
  }
}

char *from_hex_to_dec(char *info)
{
}

char *from_dec_to_hex(char *info)
{
}

uint8_t get_checksum(struct GenericPacket packet)
{
  uint8_t *p = (uint8_t *)(void *)&packet;
  size_t to_check_info_size = sizeof(struct GenericPacket) - sizeof(uint8_t);
  uint8_t checksum = 0;
  // printf("Generic packet size -> %ld\n", to_check_info_size + 1);
  for (int i = 0; i < 100; i++)
  {
    checksum ^= p[i];
    // printf("At point: %d Checksum is-> %u\n", i, checksum);
  }
  // printf("checksum-> %u\n", checksum);
  return checksum;
}
/// type: 0 - client, 1(every other int) - server
/// id: client id is necessary if this is server. If client then NULL
void process_incoming_packet(int socket, int type, int id)
{
  char in[1];
  size_t decoded_length = 0;
  char out[DOUBLE_OUT];
  char received_info[DOUBLE_OUT];
  int binaryZero = 0;
  // printf("Processing client id =%d, socket=%d\n", id, socket);
  // printf("Client count=%d\n", *client_count);

  while (1)
  {
    if (read(socket, in, 1) == 0 && type == 1)
    {
      exit(0);
    }
    binaryZero = 0;
    if (in[0] != '\0')
    {
      out[0] = in[0];
      for (size_t i = 1; i < DOUBLE_OUT; i++)
      {
        if (read(socket, in, 1) == 0 && type == 1)
        {
          exit(0);
        }
        if (in[0] == '\0')
        {
          if (binaryZero < 0) // Binary zero was before. Need to drop the packet till next two binary zeroes
          {
            binaryZero--;
            if (binaryZero == -3) // Two binary zeroes detected after malformed packet. Can return now
            {
              break;
            }
          }
          else
          {
            binaryZero++; // Binary zero received
          }
          if (binaryZero == 2)
          {
            out[--i] = '\0';
            void *decoded_info = decode(out, &decoded_length); // free out?
            memcpy(received_info, decoded_info, decoded_length);
            printf("Decoded size: %ld\n", decoded_length);
            if (type == 0)
            {
              process_packet_client(socket, &received_info);
            }
            else
            {
              process_packet_server(socket, (void *)&received_info, id);
            }
            break;
          }
        }
        else if (binaryZero == 0) // No binary zeroes detected
        {
          out[i] = in[0];
        }
        else if (binaryZero > 0 || binaryZero < 0) // Binary zero was before. Need to drop the packet till next two binary zeroes (Always to -1 if altering 00 06 00 20 00)
        {
          binaryZero = -1;
        }
      }
    }
  }
}
