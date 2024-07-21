#include <stdio.h>
#include <stdlib.h> // exit(), size_t, malloc
#include "graphical/text_formatter.h" // print_failure()
#include "packets/connection.h"
#include "packets/shared_packets.h"

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
    // TODO: Remove socket blocking and if no connection go to main gameloop.
    int client_socket = accept_connection(server_tcp_socket);
    // TODO: Place this as a status for each client to wait for HELLO. If nothing go to main gameloop.
    struct GenericPacket *client_packet = receive_generic_packet(client_socket);
    if (client_packet->packet_type != 0)
    {
        // TODO: Send ACK with player_id=0.
    }
    // TODO: Send ACK with player_id and team_id. If game started then team_id=0.
    struct HelloPacket hello_packet;
    hello_packet_deserialization(client_packet->content, &hello_packet);
    printf("Welcome client (%s) ID: %d\n", hello_packet.name, client_socket);
    printf("Client name size: %d\n", hello_packet.name_length);

    struct GenericPacket generic_packet;
    generic_packet.packet_content_size = sizeof(struct AckPacket);
    generic_packet.packet_type = 1;
    struct AckPacket ack_packet;
    ack_packet.player_id = 66;
    ack_packet.team_id = 69;
    printf("Packet contentr pointer sizeof: %ld Ack packet pointer sizeof: %ld\n", sizeof(generic_packet.content), sizeof(&ack_packet));
    generic_packet.content = (char *)&ack_packet;
    send_generic_packet(client_socket, &generic_packet);

    // THIS IS TEMPORARY!!! Just to close client connection.
    free(client_packet->content);
    free(client_packet);
    close_socket(client_socket);
    printf("Client socked with ID %d closed!\n", client_socket);
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