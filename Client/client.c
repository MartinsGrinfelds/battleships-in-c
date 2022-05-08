#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "../helpers/packet_types.h"
#include "../helpers/packet_utils.h"
#include "client_functions.h"

int main()
{
    // ncurses_test();
    int my_socket = 0;
    char buf[4256];

    char *servername;
    struct sockaddr_in remote_address;

    char inputs[100];
    char buffer[DOUBLE_OUT];
    char in[1];

    struct HELLO hello;
    int player_length = 8;
    hello.player_name_length = player_length;
    strcpy(hello.player_name, "Timothy");

    int hello_size = sizeof(uint8_t) + sizeof(char) * player_length;

    strcpy(buf, encode((void*) &hello, hello_size));
    printf("%s", &buf);

    remote_address.sin_family = AF_INET;
    remote_address.sin_port = htons(PORT);

    // servername = gethostname(HOST);
    inet_pton(AF_INET, HOST, &remote_address.sin_addr);

    if ((my_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("SOCKET ERROR\n");
        return -1;
    }

    if (connect(my_socket, (struct sockaddr *)&remote_address, sizeof(remote_address)) < 0)
    {
        printf("ERROR when making a connection.\n");
        return -1;
    }
    else
    {
        while (1)
        {
            // process_incoming_packet(my_socket, 0, 420);

            scanf("%s", inputs);
<<<<<<< HEAD
            //send(my_socket, inputs, strlen(inputs), 0);

            send(my_socket, "\0", 1, 0); // Sending binary zero
            send(my_socket, "\0", 1, 0); // As an end of packet

            //send(my_socket, inputs, strlen(inputs), 0);
            send(my_socket, buf, strlen(buf), 0);

            send(my_socket, "\0", 1, 0); // Sending binary zero
            send(my_socket, "\0", 1, 0); // As an end of packet

            send(my_socket, buf, strlen(buf), 0);

=======
            struct HELLO helloP;
            struct GENERIC_PACKET testP;
            // print_bytes((void*)&buffer, sizeof(buffer));

            memset(helloP.player_name, 0, sizeof(helloP.player_name));
            helloP.player_name_length = 8;
            strcpy(helloP.player_name, "Kaspars");

            testP.sequence_number = 10;
            testP.packet_content_size = 9;
            testP.packet_type = 0;

            // testP.content[0] = *(char*)(void*)&helloP;
            memset(testP.content, 0, sizeof(testP.content));
            memcpy(testP.content, &helloP, sizeof(helloP));
            testP.checksum = 7;
            printf("---START OF PACKET CONTENT---\n");
    printf("Sequence number: %d\n", testP.sequence_number);
    printf("Content size: %d\n", testP.packet_content_size);
    printf("Packet type: %d\n", testP.packet_type);
    printf("Checksum: %d\n", testP.checksum);
    printf("---END OF PACKET CONTENT---\n");
            //memset(buffer, 0, sizeof(buffer));
            // print_bytes((void*)&testP, sizeof(testP));
            //printf("Double out: %d\nPacket size: %d\n", DOUBLE_OUT, sizeof(testP));
            size_t encoded_length = 0;
            void* encoded_packet = encode((void *)&testP, sizeof(testP), &encoded_length); // free encoded_packet?
            memcpy(buffer, encoded_packet, encoded_length);
            //print_bytes(buffer, sizeof(testP)+1);
            //print_bytes(buffer, encoded_length);
            send(my_socket, "\0", 1, 0); // Sending binary zero
            send(my_socket, "\0", 1, 0); // As an end of packet
            send(my_socket, buffer, encoded_length, 0);
>>>>>>> mkm_dev
            send(my_socket, "\0", 1, 0); // Sending binary zero
            send(my_socket, "\0", 1, 0); // As an end of packet

            // process_incoming_packet(my_socket, 0, 420);

            // struct HELLO helloP;
        }
    }
    return 0;
}