#include <stdio.h>
#include <stdlib.h> // exit()
#include "graphical/text_formatter.h" // print_failure()
#include "packets/connection.h" 

#define PORT 12345
#define MAX_REQUESTS 20

#ifdef BATTLESHIPS_VERSION
char* APP_VERSION = BATTLESHIPS_VERSION;
#else
char* APP_VERSION = "UNDEFINED!";
#endif

int server_tcp_socket = -1;

/// @brief Does server startup actions such as socket creation, binding, listening.
void startup_server()
{
    // Call socket creation
    server_tcp_socket = create_socket();
    if (bind_socket_to_address(server_tcp_socket, PORT) < 0)
    {
        print_failure("Failed to bind socket to address!\n");
        exit(1);
    }
    if (listen_to_socket(server_tcp_socket, MAX_REQUESTS) < 0)
    {
        print_failure("Failed to listen on socket!\n");
        exit(2);
    }
}

/// @brief Registers client socket to use. Waits HELLO package and sends ACK.
void register_client()
{
    int client_socket = accept_connection(server_tcp_socket);
    struct GenericPacket *client_packet = receive_generic_packet(client_socket);
    printf("Packet received from client with socket ID: %d\n", client_socket);
    printf("Client packet Nr: %d\n", client_packet->sequence_number);
    printf("Client packet content size: %d\n", client_packet->packet_content_size);
    printf("Client packet type: %d\n", client_packet->packet_type);
    printf("Client packet checksum: %d\n", client_packet->checksum);

    // THIS IS TEMPORARY!!! Just to close client connection.
    free(client_packet->content);
    free(client_packet);
    close_socket(client_socket);
    printf("Client socked with ID %d closed!", client_socket);
}

int main()
{
    printf("Welcome to Battleships Server!\n");
    printf("Version: %s\n", APP_VERSION);
    startup_server();

    // Gameloop can start here.
    // Accept client socket and get HELLO packet.
    register_client();

    close_socket(server_tcp_socket);
    return 0;
}