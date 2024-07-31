#include <stdio.h>
#include <stdlib.h>                   // exit(), size_t, malloc
#include "graphical/text_formatter.h" // print_failure()
#include "packets/connection.h"
#include "packets/shared_packets.h"
#include <unistd.h> // sleep()
#include <string.h> // memset()

#define PORT 12345
#define MAX_REQUESTS 20
#define MAX_CLIENTS 10

#ifdef BATTLESHIPS_VERSION
char *APP_VERSION = BATTLESHIPS_VERSION;
#else
char *APP_VERSION = "UNDEFINED!";
#endif

int server_tcp_socket = -1;
int gameloop = 1, gameloop_iteration = 0, player_id = 1;
struct ClientInfo
{
    int socket_nr;
    int id;
    int status;
    char username[MAX_USERNAME];
};
struct ClientInfo clients[MAX_CLIENTS];

/// @brief Does server startup actions such as socket creation, binding, listening.
void startup_server()
{
    memset(&clients, -1, sizeof(struct ClientInfo) * MAX_CLIENTS);
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
    set_socket_non_blocking(server_tcp_socket);
}

/// @brief Adds info to list of clients about client socket and its initial status.
/// @param socket File descriptor for socket.
/// @return 0 - if success, 1 - socket nr invalid, 2 - full server.
int add_client_socket_info(int socket)
{
    if (socket < 0)
    {
        // Socket number below 0 is invalid.
        return 1;
    }
    size_t i = 0;
    while (i < MAX_CLIENTS)
    {
        if (clients[i].socket_nr == -1)
        {
            clients[i].socket_nr = socket;
            clients[i].status = 0;
            return 0;
        }
        i++;
    }
    // Clients too much.
    return 2;
}

/// @brief Try to register client username.
/// @param username Where to save received username.
/// @param user_id Where to save user ID.
/// @return 1 if succesful registration. 0 waiting on client.
int register_client_username(int socket, char* username, int* user_id)
{

    struct GenericPacket *client_packet = receive_generic_packet(socket);
    if (client_packet == NULL)
    {
        // printf("Waiting on client %d\n", socket);
        return 0;
    }
    
    // if (client_packet->packet_type != 0)
    // {
    //     // TODO: Send ACK with player_id=0.
    // }
    // TODO: Send ACK with player_id and team_id. If game started then team_id=0.
    struct HelloPacket hello_packet;
    hello_packet_deserialization(client_packet->content, &hello_packet);
    strcpy(username, hello_packet.name);

    struct GenericPacket generic_packet;
    generic_packet.packet_content_size = sizeof(struct AckPacket);
    generic_packet.packet_type = 1;
    struct AckPacket ack_packet;
    *user_id = player_id++;
    ack_packet.player_id = *user_id;
    ack_packet.team_id = 69;
    generic_packet.content = (char *)&ack_packet;
    printf("Welcome client (%s) ID: %d Socket: %d\n", hello_packet.name, *user_id, socket);
    send_generic_packet(socket, &generic_packet);

    // THIS IS TEMPORARY!!! Just to close client connection.
    free(client_packet->content);
    free(client_packet);
    return 1;
}

/// @brief Iterates through clients and tries to register them.
void get_clients_usernames()
{
    size_t i = 0;
    while (i < MAX_CLIENTS)
    {
        if (clients[i].status == 0 && clients[i].socket_nr >= 0)
        {
            if(register_client_username(clients[i].socket_nr, clients[i].username, &clients[i].id))
            {
                clients[i].status++;
            }
        }
        i++;
    }
    
}

/// @brief Does client registration and their status updates.
/// Waits HELLO package and sends ACK.
/// All actions should be non-blocking here so that main game loop is not affected.
void register_clients()
{
    int client_socket = accept_connection(server_tcp_socket);
    switch (client_socket)
    {
    case -1:
        print_failure("Failure while accepting connection\n");
        break;
    case -2:
        // printf("No client connections waiting.\n");
        break;
    default:
        // Set client non-blocking to not halt server.
        set_socket_non_blocking(client_socket);
        switch (add_client_socket_info(client_socket))
        {
        case 0:
            printf("Saved client socket Nr: %d\n", client_socket);
            break;
        case 2:
            printf("Too much clients saved already!\n");
            break;
        default:
            print_failure("Impossible failure!\n");
        }
    }
    get_clients_usernames();
    return;
}

/// @brief Do all required actions within loop.
/// @return
int serverloop()
{
    printf("Starting server loop!\n");
    while (gameloop)
    {
        gameloop_iteration++;
        sleep(1);
        printf("Loop iteration: %d\n", gameloop_iteration);
        if (gameloop_iteration > 25)
        {
            gameloop = 0;
        }
        register_clients();

        // Here is place for other server functionality.
    }
    return 0;
}

/// @brief Server cleanup actions. Memory cleaning, socket disconnections.
void server_cleanup()
{
    printf("Cleanup start!\n");
    size_t i = 0;
    while (i < MAX_CLIENTS)
    {
        if (clients[i].socket_nr >= 0)
        {
            close_socket(clients[i].socket_nr);
            printf("Client socked with ID %d and name %s closed!\n", clients[i].socket_nr, clients[i].username);
        }
        i++;
    }
    printf("Cleanup end!\n");
}

int main()
{
    printf("Welcome to Battleships Server!\n");
    printf("Version: %s\n", APP_VERSION);
    startup_server();

    // Gameloop can start here.
    // Accept client socket and get HELLO packet.
    serverloop();

    server_cleanup();
    close_socket(server_tcp_socket);
    return 0;
}