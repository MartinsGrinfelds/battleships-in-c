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
#define WAIT_UNTIL_NEXT_STAGE 15 // How long to wait for more players in lobby
#define NUM_SHIP_TYPES 1

#ifdef BATTLESHIPS_VERSION
char *APP_VERSION = BATTLESHIPS_VERSION;
#else
char *APP_VERSION = "UNDEFINED!";
#endif

int server_tcp_socket = -1;
int gameloop = 1, gameloop_iteration = 0, player_id = 1, player_count_changed = 0;

// Demo turn-manager settings (minimal, non-invasive)
#define DEMO_ACTIONS_CAP 10
#define ACTION_TIMEOUT_SECONDS 15

static int turn_order[MAX_CLIENTS];
static int turn_order_len = 0;     // number of entries in turn_order
static int active_turn_pos = -1;   // index into turn_order (-1 means none)
typedef enum { TURN_INACTIVE = 0, TURN_AWAITING_ACTION = 1 } TurnState;
static TurnState turn_state = TURN_INACTIVE;
static int turn_timeout_remaining = 0; // seconds remaining for current turn
static int demo_action_count = 0;       // total accepted demo actions

// Settings
#define DUPLICATE_USERNAMES "Someone has Taken This Username. Try Another one."
#define INVALID_USERNAME_SPACE_AT_END "Username Cannot End With Space!"

// Status messages
#define STATE_MESSAGE_LOBBY "Game in Lobby.\n\nWaiting for players..."
#define STATE_MESSAGE_SHIP_PLACEMENT "Place your ships!"
#define STATE_MESSAGE_WAITING_ON_PLAYERS "Waiting on others..."

static bool waiting_for_placement = false;

struct Player clients[MAX_CLIENTS] = {0}; // Initialize to 0 for correct name printing.

struct StatePacket live_state; // TODO: REMOVE it is test

/// @brief Checks if a single player has placed all their ships.
/// @param player_id The ID of the player to check.
/// @return true if the player has placed all ships, false otherwise.
int player_has_placed_all_ships(uint8_t player_id)
{
    if (!live_state.map_objects || live_state.object_count == 0)
    {
        return false;
    }

    int ship_types_found[NUM_SHIP_TYPES] = {0};
    int found_count = 0;

    for (int i = 0; i < live_state.object_count; i++)
    {
        struct MapObject *obj = &live_state.map_objects[i];
        if (obj->player_id == player_id && obj->object_type >= 1 && obj->object_type <= 5) // need to get rid of these magic numbers at some point somehow
        {
            if ((!ship_types_found[obj->object_type - 1]) == 1)
            {
                ship_types_found[obj->object_type - 1] = 1;
                found_count++;
            }
        }
    }
    return (found_count == NUM_SHIP_TYPES);
}

/// @brief Checks if all active players have placed all their ships.
/// @return true if all players have placed all ships, false otherwise.
int all_players_placed_ships()
{
    for (int i = 0; i < live_state.player_count; i++)
    {
        if (!player_has_placed_all_ships(live_state.players[i].id))
        {
            return false;
        }
    }
    return true;
}

/// @brief Does server startup actions such as socket creation, binding, listening.
void startup_server()
{
    // TODO: Improve map size configuration.
    live_state.map_width = 50;
    live_state.map_height = 50;
    live_state.status = 0;
    live_state.object_count = 1;
    live_state.map_objects = malloc(sizeof(struct MapObject) * 1);
    live_state.map_objects[0].object_type = 1;
    live_state.map_objects[0].x = 25;
    live_state.map_objects[0].y = 25;
    live_state.map_objects[0].rotation = 0;
    live_state.map_objects[0].team_id = 0;
    live_state.player_count = 0;
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

char *username_invalid(char *username)
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
int register_client_username(int socket, char *username, uint8_t *user_id, uint8_t *team_id)
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
            if (register_client_username(clients[i].socket_nr, clients[i].username, &clients[i].id, &clients[i].team_id))
            {
                if (live_state.status == 0)
                {
                    // Game still in lobby.
                    // TODO: Check if team is configured.
                    clients[i].status = 2; // Registered
                }
                else
                {
                    // TODO: Check if team is configured.
                    clients[i].status = 3; // Observer
                }
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
        print_failure("Failure while accepting client connection\n");
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
    // if player count changed, that means it is now either
    // 1) below the minimum threshold which means the timer is not needed and game state gets set back to lobby
    // or
    // 2) we need to reset the timer due to players joining / disconnecting
    if (player_count_changed && live_state.status == 0)
    {
        if (live_state.player_count < MINIMUM_PLAYER_COUNT)
        {
            stop_timer();
        }
        else
        {
            start_or_reset_timer(WAIT_UNTIL_NEXT_STAGE);
        }
    }
    // if player count did not change, don't do anything unless the timer was already running and has finished
    // in which case game state moves to ship placement
    else if (live_state.status == 0)
    {
        if (timer_has_finished_running())
        {
            live_state.status = 1;
            print_success("Lobby is closing. Players in game: ");
            printf("%d\n", live_state.player_count);
        }
    }
    else if (live_state.status == 1)
    {
        if (handle_ship_placement())
        {
            live_state.status = 2;
            print_success("All players have placed their ships. Game is starting!\n");
        }
    }
    return;
}

char *get_state_message()
{
    switch (live_state.status)
    {
    case 0:
        return STATE_MESSAGE_LOBBY;
    case 1:
        return STATE_MESSAGE_SHIP_PLACEMENT;
    case 2:
        return STATE_MESSAGE_WAITING_ON_PLAYERS;
    default:
        return "GAME BROKE!";
    }
}

/// @brief Update active and connected clients with game state.
void send_everyone_state()
{
    struct GenericPacket generic_packet;
    struct MessagePacket message;
    message.message_type = 3;
    message.sender_id = 0;
    char *state_message = get_state_message();
    message.message = state_message;
    message.message_length = strlen(state_message);
    size_t i = 0;
    while (i < MAX_CLIENTS)
    {
        if (clients[i].status > 1 && clients[i].status < 4)
        {
            // TODO: I am not sure what is happening here
            // Probably need to send each player state based on their team and status HERE
            generic_packet.packet_type = 3;
            generic_packet.content = state_packet_serialization(&live_state, &generic_packet.packet_content_size);
            if (send_generic_packet(clients[i].socket_nr, &generic_packet))
            {
                print_warning("WARNING: Client Disconnected ");
                printf("Socket(%d)\n", clients[i].socket_nr);
                close_socket(clients[i].socket_nr);
                clients[i].status = 4;
            }
            free(generic_packet.content);
            generic_packet.packet_type = 2;
            generic_packet.content = message_packet_serialization(&message, &generic_packet.packet_content_size);
            if (send_generic_packet(clients[i].socket_nr, &generic_packet))
            {
                print_warning("WARNING: Client Disconnected ");
                printf("Socket(%d)\n", clients[i].socket_nr);
                close_socket(clients[i].socket_nr);
                clients[i].status = 4;
            }
            free(generic_packet.content);
        }
        i++;
    }
}

int get_player_id_from_socket(int socket)
{
    for (size_t i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].socket_nr == socket)
        {
            return clients[i].id;
        }
    }
    return -1;
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
        // IPlace packet process
        struct IPlacePacket i_place_packet;
        memcpy(&i_place_packet, client_packet->content, sizeof(struct IPlacePacket));
        printf("Client placed ship type %d at X:%d Y:%d\n", i_place_packet.object.object_type, i_place_packet.object.x, i_place_packet.object.y);

        // Now add object to live state.
        if (live_state.object_count == 0)
        {
            live_state.map_objects = malloc(sizeof(struct MapObject) * 1);
            live_state.object_count = 1;
        }
        else
        {
            live_state.map_objects = realloc(live_state.map_objects, sizeof(struct MapObject) * (live_state.object_count + 1));
            live_state.object_count++;
        }
        live_state.map_objects[live_state.object_count - 1].object_type = i_place_packet.object.object_type;
        live_state.map_objects[live_state.object_count - 1].x = i_place_packet.object.x;
        live_state.map_objects[live_state.object_count - 1].y = i_place_packet.object.y;
        live_state.map_objects[live_state.object_count - 1].rotation = i_place_packet.object.rotation;
        live_state.map_objects[live_state.object_count - 1].team_id = 1; // TODO: Get team from client.
        live_state.map_objects[live_state.object_count - 1].player_id = get_player_id_from_socket(socket);
        live_state.map_objects[live_state.object_count - 1].info = 0;

        // Signal that the placement has been received
        waiting_for_placement = false;

        return 2;
    case 7:
    {
        // IGo packet process (demo): accept only from active player
        struct IGoPacket i_go_packet;
        memcpy(&i_go_packet, client_packet->content, sizeof(struct IGoPacket));
        int sender_id = get_player_id_from_socket(socket);

        int active_client_slot = -1;
        if (active_turn_pos >= 0 && active_turn_pos < turn_order_len)
        {
            active_client_slot = turn_order[active_turn_pos];
        }

        if (sender_id < 0)
        {
            printf("IGo: unknown sender socket %d\n", socket);
        }
        else if (active_client_slot == -1 || clients[active_client_slot].id != sender_id)
        {
            printf("IGo from non-active player id=%d (socket %d) ignored\n", sender_id, socket);
        }
        else if (turn_state != TURN_AWAITING_ACTION)
        {
            printf("IGo received but server not awaiting action.\n");
        }
        else
        {
            printf("Demo action received from player id=%d (slot %d)\n", sender_id, active_client_slot);
            // MVP: accept the action, increment demo action count, end turn
            demo_action_count++;
            turn_state = TURN_INACTIVE;
            turn_timeout_remaining = 0;
        }

        if (client_packet->content) free(client_packet->content);
        free(client_packet);
        return 2;
    }
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

// Rebuild the turn order from currently active clients (status == 2).
void rebuild_turn_order(void)
{
    turn_order_len = 0;
    for (int i = 0; i < MAX_CLIENTS && turn_order_len < MAX_CLIENTS; ++i)
    {
        if (clients[i].status == 2)
        {
            turn_order[turn_order_len++] = i;
        }
    }
    if (turn_order_len == 0)
    {
        active_turn_pos = -1;
        turn_state = TURN_INACTIVE;
    }
    if (active_turn_pos >= turn_order_len) active_turn_pos = -1;
    // Log rebuilt turn order
    printf("Rebuilt turn order: %d players\n", turn_order_len);
    if (turn_order_len > 0)
    {
        printf("Turn slots: ");
        for (int j = 0; j < turn_order_len; ++j)
        {
            int slot = turn_order[j];
            if (slot >= 0 && slot < MAX_CLIENTS)
            {
                printf("[slot %d -> id=%d name=%s] ", slot, clients[slot].id, clients[slot].username);
            }
        }
        printf("\n");
    }
}

// Send YouGo packet (packet type 6) to client at clients[client_index].
void send_yougo_to_client_index(int client_index)
{
    if (client_index < 0 || client_index >= MAX_CLIENTS) return;
    struct Player *p = &clients[client_index];
    if (p->status != 2) return;

    struct YouGoPacket you_go;
    you_go.player_id = p->id;

    struct GenericPacket gp;
    gp.packet_type = 6; // YouGo
    gp.packet_content_size = sizeof(struct YouGoPacket);
    gp.content = (char *)&you_go;
    gp.checksum = 0;

    if (send_generic_packet(p->socket_nr, &gp) == 0)
    {
        printf("Sent YouGo to player %d (%s)\n", p->id, p->username);
        turn_state = TURN_AWAITING_ACTION;
        turn_timeout_remaining = ACTION_TIMEOUT_SECONDS;
    }
    else
    {
        printf("Player disconnected while sending YouGo: Socket(%d)\n", p->socket_nr);
        close_socket(p->socket_nr);
        p->status = 4;
        rebuild_turn_order();
    }
}

// Per-tick turn manager for demo: init, send YouGo, handle timeout/advance.
void turn_manager_tick(void)
{
    if (demo_action_count >= DEMO_ACTIONS_CAP)
    {
        // Broadcast end of demo and reset to lobby
        // Implemented in demo_finish()
        extern void demo_finish(void);
        demo_finish();
        return;
    }

    if (turn_order_len < 2)
    {
        rebuild_turn_order();
        if (turn_order_len < 2) return; // need 2 players for demo
    }

    if (turn_state == TURN_INACTIVE)
    {
        if (active_turn_pos == -1) active_turn_pos = 0;
        else active_turn_pos = (active_turn_pos + 1) % turn_order_len;
        int client_idx = turn_order[active_turn_pos];
        printf("Turn manager: Starting turn for player id=%d (client slot %d)\n", clients[client_idx].id, client_idx);
        send_yougo_to_client_index(client_idx);
        return;
    }

    if (turn_state == TURN_AWAITING_ACTION)
    {
        if (turn_timeout_remaining > 0)
        {
            turn_timeout_remaining--;
            if (turn_timeout_remaining <= 0)
            {
                int client_idx = turn_order[active_turn_pos];
                printf("Turn timed out for player id=%d (slot %d). Skipping.\n", clients[client_idx].id, client_idx);
                demo_action_count++;
                turn_state = TURN_INACTIVE; // next tick will advance
            }
        }
    }
}

// Broadcast EndGame to all active players, reset demo state and return to lobby.
void demo_finish(void)
{
    printf("Demo finished after %d actions. Broadcasting EndGame.\n", demo_action_count);
    struct EndGamePacket eg;
    eg.winner_id = 0;
    char *msg = "Demo finished";
    eg.message_length = (uint8_t)strlen(msg);
    eg.message = msg;

    struct GenericPacket gp;
    gp.packet_type = 8; // EndGame
    gp.content = endgame_packet_serialization(&eg, &gp.packet_content_size);
    gp.checksum = 0;

    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (clients[i].status == 2 && clients[i].socket_nr >= 0)
        {
            if (send_generic_packet(clients[i].socket_nr, &gp))
            {
                print_warning("WARNING: Client Disconnected ");
                printf("Socket(%d)\n", clients[i].socket_nr);
                close_socket(clients[i].socket_nr);
                clients[i].status = 4;
            }
        }
    }
    free(gp.content);

    // Reset demo state and return to lobby
    demo_action_count = 0;
    turn_state = TURN_INACTIVE;
    active_turn_pos = -1;
    rebuild_turn_order();
    live_state.status = 0; // Back to lobby
    print_success("Demo ended; returning to lobby.\n");
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
        if (gameloop_iteration % 100 == 0)
        {
            // Info about 100 iterations passed.
            printf("Loop iteration: %d\n", gameloop_iteration);
        }
        register_clients();
        update_state_with_players();
        update_game_status();
        send_everyone_state();
        process_incoming_packets();

        // Turn manager tick for demo two-player flow
        if (live_state.status == 2)
        {
            turn_manager_tick();
        }

        // DEMO: move ship up
        if (live_state.object_count > 0)
        {
            live_state.map_objects[0].y = (live_state.map_objects[0].y + 1);
            if (live_state.map_objects[0].y >= live_state.map_height - 6)
            {
                live_state.map_objects[0].y = 5;
            }
        }

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

/// @brief Manages the entire ship placement phase in a turn-based manner.
/// @return Returns 1 if the placement phase is complete, 0 otherwise.
int handle_ship_placement()
{
    static int placement_turn_player_index = 0;
    static bool placement_initialized = false;

    // One-time initialization for the placement phase
    if (!placement_initialized)
    {
        printf("Initializing ship placement phase.\n");
        placement_turn_player_index = 0;
        waiting_for_placement = false;
        placement_initialized = true;
    }

    // If all players have placed their ships, report completion.
    if (all_players_placed_ships())
    {
        placement_initialized = false; // Reset for a potential new game
        print_success("All players have placed their ships. Starting game!\n");
        return 1; // Signal completion
    }

    // If we are waiting for a player to place a ship, do nothing in this tick.
    if (waiting_for_placement)
    {
        printf("Waiting for placement...\n");
        return 0;
    }

    // Find the next active player who needs to place a ship and send them the packet
    int initial_player_index = placement_turn_player_index;
    do
    {
        struct Player *player = &clients[placement_turn_player_index];

        // Check if the player is active and has not yet placed all their ships
        if (player->status == 2 && !player_has_placed_all_ships(player->id))
        {
            struct YouPlacePacket you_place_packet;
            you_place_packet.object_type = 1; // Only one ship type for now

            struct GenericPacket generic_packet;
            generic_packet.packet_type = 4;
            generic_packet.packet_content_size = sizeof(struct YouPlacePacket);
            generic_packet.content = (char *)&you_place_packet;
            generic_packet.checksum = 0;

            if (send_generic_packet(player->socket_nr, &generic_packet) == 0)
            {
                printf("Sent YouPlace to player %d (%s)\n", player->id, player->username);
                waiting_for_placement = true; // Wait for the IPlace response
            }
            else
            {
                // Handle disconnection
                printf("Player disconnected during ship placement: Socket(%d)\n", player->socket_nr);
                close_socket(player->socket_nr);
                player->status = 4; // Mark as disconnected
            }
            printf("Still in progress.\n");
            return 0; // Still in progress
        }

        // Move to the next player and loop
        placement_turn_player_index = (placement_turn_player_index + 1) % MAX_CLIENTS;
        printf("placement_turn_player_index: %d\n", placement_turn_player_index);
    } while (placement_turn_player_index != initial_player_index);

    printf("Still in progress, end.\n");
    return 0; // Still in progress
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
