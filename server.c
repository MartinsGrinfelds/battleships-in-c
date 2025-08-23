#include <stdio.h>
#include <stdlib.h>                   // exit(), size_t, malloc
#include "graphical/text_formatter.h" // print_failure()
#include "packets/connection.h"
#include "packets/shared_packets.h"
#include "utils/timer.h"
#include <unistd.h> // sleep()
#include <string.h> // memset()

#define PORT 12345
#define MAX_REQUESTS 20
#define MAX_CLIENTS 10
#define MINIMUM_PLAYER_COUNT 2
#define WAIT_UNTIL_NEXT_STAGE 30

#ifdef BATTLESHIPS_VERSION
char *APP_VERSION = BATTLESHIPS_VERSION;
#else
char *APP_VERSION = "UNDEFINED!";
#endif

int server_tcp_socket = -1;
int gameloop = 1, gameloop_iteration = 0, player_id = 1, player_count_changed = 0;

// Settings
#define DUPLICATE_USERNAMES "Someone has Taken This Username. Try Another one."
#define INVALID_USERNAME_SPACE_AT_END "Username Cannot End With Space!"

struct Player clients[MAX_CLIENTS] = {0}; //Initialize to 0 for correct name printing.


struct StatePacket live_state; // TODO: REMOVE it is test
/// @brief Does server startup actions such as socket creation, binding, listening.
void startup_server()
{
    // DEMO BLOCK
    live_state.map_width = 50;
    live_state.map_height = 50;
    live_state.status = 0;
    live_state.object_count = 1;
    live_state.player_count = 0;
    live_state.map_objects = calloc(1, sizeof(struct MapObject));
    live_state.map_objects->object_type = 1; // Base ship
    live_state.map_objects->team_id = 0;
    live_state.map_objects->rotation = 1;
    live_state.map_objects->x = 20;
    live_state.map_objects->y = 30;
    memset(&clients, 0, sizeof(struct Player) * MAX_CLIENTS);
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
        if (clients[i].status == 0)
        {
            clients[i].socket_nr = socket;
            clients[i].status = 1;
            clients[i].team_id = 0;
            return 0;
        }
        i++;
    }
    // Clients too much.
    return 2;
}

/// @brief Checks if client with such username is already registered.
/// @param username Username to check.
/// @return 0 if no player with such username found. Anything above 0 is user ID which has same username.
int username_exists(char *username)
{
    size_t i = 0;
    while (i < MAX_CLIENTS)
    {
        if (clients[i].status != 0)
        {
            if (strcmp(username, clients[i].username) == 0)
            {
                return clients[i].id;
            }
        }
        i++;
    }
    return 0;
}

char *username_invalid(char* username)
{
    if (username[strlen(username) - 1] == ' ')
    {
        return INVALID_USERNAME_SPACE_AT_END;
    }
    return NULL;
}

/// @brief Try to register client username.
/// @param username Where to save received username.
/// @param user_id Where to save user ID.
/// @return 1 if succesful registration. 0 waiting on client.
int register_client_username(int socket, char* username, uint8_t* user_id, uint8_t* team_id)
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
    int registration_status = 0;
    struct HelloPacket hello_packet;
    hello_packet_deserialization(client_packet->content, &hello_packet);
    struct AckPacket ack_packet;
    ack_packet.player_id = 0;
    ack_packet.team_id = 0;
    ack_packet.message_length = 0;
    // Check if username isn't already registered. Or isn't invalid.
    char *is_invalid = username_invalid(hello_packet.name);
    if (is_invalid)
    {
        // Username invalid.
        ack_packet.message_length = strlen(is_invalid);
        ack_packet.message = is_invalid; // We can do this because message is already defined in memory.
    }
    else if (username_exists(hello_packet.name))
    {
        // Username duplicates.
        ack_packet.message_length = strlen(DUPLICATE_USERNAMES);
        ack_packet.message = DUPLICATE_USERNAMES; // We can do this because message is already defined in memory.
    }
    else
    {
        // Succesful registration.
        // TODO: Check game status OBSERVER?
        strcpy(username, hello_packet.name);
        *user_id = player_id++;
        ack_packet.player_id = *user_id;
        ack_packet.team_id = *team_id;
        printf("Welcome client (%s) ID: %d Socket: %d\n", hello_packet.name, *user_id, socket);
        registration_status = 1;
    }
    struct GenericPacket generic_packet;
    generic_packet.packet_type = 1;
    generic_packet.content = ack_packet_serialization(&ack_packet, &generic_packet.packet_content_size);
    send_generic_packet(socket, &generic_packet);

    free(generic_packet.content);
    free(client_packet->content);
    free(client_packet);
    return registration_status;
}

/// @brief Iterates through clients and tries to register them.
void get_clients_usernames()
{
    size_t i = 0;
    while (i < MAX_CLIENTS)
    {
        if (clients[i].status == 1 && clients[i].socket_nr >= 0)
        {
            if(register_client_username(clients[i].socket_nr, clients[i].username, &clients[i].id, &clients[i].team_id))
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
    // Following function can be included also in different place but is here as related to client registration.
    // Previous connection will be accepted but user won't be that fast to enter name in one tick
    get_clients_usernames();
    return;
}

void update_state_with_players()
{
    // TODO: optimize if no change then do not update!!!
    int active_player_count = 0;
    size_t i = 0, to_i = 0;
    while (i < MAX_CLIENTS)
    {
        if (clients[i].status > 1 && clients[i].status < 4)
        {
            active_player_count++;
        }
        i++;
    }
    if (live_state.players)
    {
        free(live_state.players);
    }
    // DANGER IF RUN IN PARRALLEL!!!
    live_state.players = calloc(active_player_count, sizeof(struct Player));
    player_count_changed = (live_state.player_count != active_player_count) ? 1 : 0;
    live_state.player_count = active_player_count;
    i = 0;
    while (i < MAX_CLIENTS)
    {
        if (clients[i].status > 1 && clients[i].status < 4)
        {
            live_state.players[to_i].socket_nr = clients[i].socket_nr; // This is not required for each client but only server.
            live_state.players[to_i].id = clients[i].id;
            live_state.players[to_i].team_id = clients[i].team_id;
            live_state.players[to_i].status = clients[i].status;
            strcpy(live_state.players[to_i++].username, clients[i].username);
        }
        i++;
    }
}

void update_game_status()
{
    // if player count changed, that means it is now either below the minimum threshold
    // which means the timer is not needed and game state gets set back to lobby
    // otherwise we need to reset the timer
    if (player_count_changed)
    {
        if (live_state.player_count < MINIMUM_PLAYER_COUNT)
        {
            live_state.status = 0;
            stop_timer();
        }
        else
        {
            start_or_reset_timer(WAIT_UNTIL_NEXT_STAGE);
        }
    }
    // if player count did not change, don't do anything unless the timer was already running and has finished
    // in which case game state moves to ship placement
    else
    {
        if (timer_has_finished_running())
        {
            live_state.status = 1;
        }
    }
    return;
}

/// @brief Update active and connected clients with game state.
void send_everyone_state()
{
    struct GenericPacket generic_packet;
    struct MessagePacket message;
    message.message_type = 3;
    message.sender_id = 0;
    message.message = "DEMO state";
    message.message_length = sizeof("DEMO state");
    size_t i = 0;
    while (i < MAX_CLIENTS)
    {
        if (clients[i].status > 1 && clients[i].status < 4)
        {
            generic_packet.packet_type = 3;
            generic_packet.content = state_packet_serialization(&live_state, &generic_packet.packet_content_size);
            if(send_generic_packet(clients[i].socket_nr, &generic_packet))
            {
                print_warning("WARNING: Client Disconnected ");
                printf("Socket(%d)\n", clients[i].socket_nr);
                close_socket(clients[i].socket_nr);
                clients[i].status = 4;
            }
            free(generic_packet.content);
            generic_packet.packet_type = 2;
            generic_packet.content = message_packet_serialization(&message, &generic_packet.packet_content_size);
            if(send_generic_packet(clients[i].socket_nr, &generic_packet))
            {
                print_warning("WARNING: Client Disconnected "); // TODO: remove duplicate code
                printf("Socket(%d)\n", clients[i].socket_nr);
                close_socket(clients[i].socket_nr);
                clients[i].status = 4;
            }
            free(generic_packet.content);
        }
        i++;
    }
}

int process_client_packet(int socket)
{
    struct GenericPacket *client_packet = receive_generic_packet(socket);
    if (client_packet == NULL)
    {
        // TODO: Print debug info
        return 1;
    }
    switch (client_packet->packet_type)
    {
    case 0:
        // TODO: HELLO packet process move implementation from top to here?
        print_failure("Client sent HELLO packet on individual processing. NEED TO MOVE IMPLEMENTATION! Socket: ");
        printf("%d\n", socket);
        return 2;
    case 2:
        // TODO: MESSAGE packet process
        print_failure("Not implemented: ");
        printf("Client sent MESSAGE packet on individual processing. Socket: %d\n", socket);
        return 2;
    case 5:
        // TODO: IPlace packet process
        print_failure("Not implemented: ");
        printf("Client sent IPlace packet on individual processing. Socket: %d\n", socket);
        return 2;
    case 7:
        // TODO: IGo packet process
        print_failure("Not implemented: ");
        printf("Client sent IGo packet on individual processing. Socket: %d\n", socket);
        return 2;
    default:
        // TODO: Implement reporting issue to client if reasonable
        print_failure("Unknown packet type received from client: ");
        printf("%d\n", client_packet->packet_type);
        print_failure("Socket: ");
        printf("%d\n", socket);
        return 2;
    }
    free(client_packet->content);
    free(client_packet);
    return 0;
}

void process_incoming_packets()
{
    size_t i = 0;
    while (i < MAX_CLIENTS)
    {
        if (clients[i].status == 2 && clients[i].socket_nr >= 0)
        {
            process_client_packet(clients[i].socket_nr);
        }
        i++;
    }
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
        if(gameloop_iteration%100 == 0)
        {
            // Info about 100 iterations passed.
            printf("Loop iteration: %d\n", gameloop_iteration);
        }
        register_clients();
        update_state_with_players();
        update_game_status();
        send_everyone_state();

        // TEST BLOCK START (to be removed)
        live_state.map_objects->x += 1;
        if (live_state.map_objects->x > 40)
        {
            live_state.map_objects->x = 5;
        }
        
        live_state.map_objects->y += 1;
        if (live_state.map_objects->y > 41)
        {
            live_state.map_objects->y = 5;
        }
        // TEST BLOCK END
        process_incoming_packets();

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
        if (clients[i].socket_nr > 0)
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
