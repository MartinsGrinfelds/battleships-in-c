#include <stdio.h>
#include <stdlib.h> // exit()
#include "packets/connection.h"
#include "graphical/text_formatter.h" // print_failure()

#define HOST "127.0.0.1"
#define PORT 12345

#ifdef BATTLESHIPS_VERSION
char* APP_VERSION = BATTLESHIPS_VERSION;
#else
char* APP_VERSION = "UNDEFINED!";
#endif

int client_tcp_socket = -1;

/// @brief Does client startup actions such as socket creation, binding, connection
void startup_client()
{
    // Call socket creation
    client_tcp_socket = create_socket();
    if(connect_socket(client_tcp_socket, PORT, HOST) != 0)
    {
        print_failure("Connection to server failed!\n");
        exit(1);
    }
    // Here probably UI should be launched.
    struct GenericPacket test_packet;
    test_packet.sequence_number = 1;
    test_packet.packet_content_size = 0;
    test_packet.packet_type = 13;
    test_packet.checksum = 69;
    send_generic_packet(client_tcp_socket, &test_packet);

    // THIS IS TEMPORARY!!! Just to close client connection.
    close_socket(client_tcp_socket);
}

int main()
{
    printf("Welcome to Battleships Client!\n");
    printf("Version: %s\n", APP_VERSION);

    startup_client();
    // After startup initiate HELLO packket.
    // Game loop can start here.
    return 0;

}