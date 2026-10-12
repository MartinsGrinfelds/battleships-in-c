#include <stdio.h>
#include <stdlib.h>                   // exit(), size_t, malloc
#include "graphical/text_formatter.h" // print_failure(), log_message()
#include "packets/connection.h"
#include "packets/shared_packets.h"
#include "game/game_logic.h"
#include "utils/timer.h"
#include "utils/config.h"
#include <unistd.h> // sleep()
#include <string.h> // memset()
#include <signal.h> // signal()
#include <poll.h>   // poll()
#include <stdarg.h> // va_list
#include <ctype.h>  // isprint()

#define MAX_REQUESTS 20
#define MAX_CLIENT_SLOTS (CONFIG_MAX_PLAYERS * 2) // Disconnected players keep their slot during game.
#define MAX_SHIPS (CONFIG_MAX_PLAYERS * SHIP_TYPE_COUNT)
#define SHOT_MARKERS_PER_TEAM 20 // Last shots that team remembers (shown on map).
#define MAX_MARKERS (SHOT_MARKERS_PER_TEAM * MAX_TEAMS)
#define REGISTRATION_TIMEOUT_SECONDS 120 // New connection must send HELLO within this time.
#define STATE_UPDATE_INTERVAL_MS 1000 // Wiki: Regular update messages are sent out 1 time a second.
#define MIN_STATE_INTERVAL_MS 100 // Changes are sent quicker but not more often than this.
#define LOOP_POLL_MS 50

#ifdef BATTLESHIPS_VERSION
char *APP_VERSION = BATTLESHIPS_VERSION;
#else
char *APP_VERSION = "UNDEFINED!";
#endif

// Settings
#define DUPLICATE_USERNAMES "Someone Has Taken This Username. Try Another One."
#define INVALID_USERNAME_SPACE_AT_END "Username Cannot Start Or End With Space!"
#define INVALID_USERNAME_LENGTH "Username Must Be 1 To 20 Characters Long!"
#define INVALID_USERNAME_CHARACTERS "Username Can Contain Only Simple Latin Characters!"
#define SERVER_FULL "Server Is Full. Try Again Later."

// Status messages
#define STATE_MESSAGE_LOBBY "Game In Lobby.\n\nWaiting For Players..."
#define STATE_MESSAGE_SHIP_PLACEMENT "Place Your Ships!"
#define STATE_MESSAGE_WAITING_ON_PLAYERS "Waiting On Others..."
#define STATE_MESSAGE_YOUR_TURN "Your Turn!"
#define STATE_MESSAGE_GAME_OVER "Game Over!"
#define STATE_MESSAGE_OBSERVER "You Are Watching."

/// @brief Server side information about client.
struct Client
{
    int in_use;
    struct Player player; // player.socket_nr is connection socket.
    struct Connection connection;
    int64_t connected_at_ms;
    uint8_t ship_to_place; // 0 - nothing to place.
    uint8_t moves_left[SHIP_TYPE_COUNT]; // Moves left this turn for each ship type.
    int hits; // For choosing winner player.
};

/// @brief Everything about the game that is going on.
struct Game
{
    uint8_t status;
    struct MapObject ships[MAX_SHIPS];
    size_t ship_count;
    struct MapObject markers[MAX_MARKERS]; // Shot markers. team_id is team that fired.
    size_t marker_count;
    struct Timer lobby_timer;
    struct Timer placement_timer;
    struct Timer turn_timer;
    struct Timer post_game_timer;
    int turn_number;
    uint8_t active_player_id;
    uint8_t current_team;
    uint8_t last_player_of_team[MAX_TEAMS + 1];
    int shots_left;
    int registered_count; // Players (not observers) registered in lobby.
    int64_t team_away_since_ms[MAX_TEAMS + 1]; // When team lost all connected players (0 - team is here).
    struct EndGamePacket result; // Last game result (sent to players who come back during game over).
    int state_changed;
    int64_t last_state_sent_ms;
};

struct ServerConfig config;
int server_tcp_socket = -1;
volatile sig_atomic_t gameloop = 1;
int gameloop_iteration = 0, player_count_changed = 0;
struct Client clients[MAX_CLIENT_SLOTS];
struct Game game;

/// @brief Stops server loop on Ctrl+C or when service manager asks to stop.
void handle_stop_signal(int signal_number)
{
    (void)signal_number;
    gameloop = 0;
}

/// @brief Does server startup actions such as socket creation, binding, listening.
void startup_server()
{
    memset(&clients, 0, sizeof(clients));
    memset(&game, 0, sizeof(game));
    size_t i = 0;
    while (i < MAX_CLIENT_SLOTS)
    {
        connection_init(&clients[i++].connection, -1);
    }
    game.status = GAME_STATUS_LOBBY;

    signal(SIGPIPE, SIG_IGN); // Writing to closed socket should not kill server.
    signal(SIGINT, handle_stop_signal);
    signal(SIGTERM, handle_stop_signal);

    // Call socket creation
    server_tcp_socket = create_socket();
    if (server_tcp_socket < 0)
    {
        exit(1);
    }
    if (bind_socket_to_address(server_tcp_socket, config.bind_address, (uint16_t)config.port) < 0)
    {
        log_message(LOG_LEVEL_ERROR, "Failed to bind socket to port %d! Is another server running?", config.port);
        exit(1);
    }
    if (listen_to_socket(server_tcp_socket, MAX_REQUESTS) < 0)
    {
        log_message(LOG_LEVEL_ERROR, "Failed to listen on socket!");
        exit(2);
    }
    set_socket_non_blocking(server_tcp_socket);
    config_print(&config);
}

// ---------------------------------------------------------------------------
// Sending helpers
// ---------------------------------------------------------------------------

/// @brief Is client connected and registered (player or observer).
int client_is_active(struct Client *client)
{
    return client->in_use && (client->player.status == PLAYER_STATUS_REGISTERED || client->player.status == PLAYER_STATUS_OBSERVER);
}

/// @brief Is client a player that takes part in current game (connected or not).
int client_is_player(struct Client *client)
{
    return client->in_use && client->player.team_id != 0 &&
           (client->player.status == PLAYER_STATUS_REGISTERED || client->player.status == PLAYER_STATUS_DISCONNECTED);
}

struct Client *find_client_by_id(uint8_t id)
{
    if (id == 0)
    {
        // 0 means server/nobody. New clients have ID 0 until they register.
        return NULL;
    }
    size_t i = 0;
    while (i < MAX_CLIENT_SLOTS)
    {
        if (clients[i].in_use && clients[i].player.id == id)
        {
            return &clients[i];
        }
        i++;
    }
    return NULL;
}

void send_packet(struct Client *client, uint8_t packet_type, char *content, size_t size)
{
    if (client->connection.socket >= 0)
    {
        connection_send(&client->connection, packet_type, content, size);
    }
    free(content);
}

void send_message(struct Client *client, uint8_t message_type, uint8_t sender_id, uint8_t receiver_id, const char *text)
{
    struct MessagePacket message;
    size_t length = strlen(text);
    message.message_type = message_type;
    message.sender_id = sender_id;
    message.receiver_id = receiver_id;
    message.message_length = (uint8_t)(length > 255 ? 255 : length);
    message.message = (char *)text;
    size_t size;
    char *content = message_packet_serialization(&message, &size);
    send_packet(client, PACKET_MESSAGE, content, size);
}

/// @brief Sends system message to one client. printf() style.
void tell(struct Client *client, const char *format, ...) __attribute__((format(printf, 2, 3)));
void tell(struct Client *client, const char *format, ...)
{
    char text[256];
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(text, sizeof(text), format, arguments);
    va_end(arguments);
    send_message(client, MESSAGE_TYPE_SYSTEM, 0, client->player.id, text);
}

/// @brief Sends system message to everyone. printf() style.
void announce(const char *format, ...) __attribute__((format(printf, 1, 2)));
void announce(const char *format, ...)
{
    char text[256];
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(text, sizeof(text), format, arguments);
    va_end(arguments);
    log_message(LOG_LEVEL_INFO, "Announce: %s", text);
    size_t i = 0;
    while (i < MAX_CLIENT_SLOTS)
    {
        if (client_is_active(&clients[i]))
        {
            send_message(&clients[i], MESSAGE_TYPE_SYSTEM, 0, 0, text);
        }
        i++;
    }
}

void send_you_place(struct Client *client)
{
    struct YouPlacePacket you_place;
    you_place.player_id = client->player.id;
    you_place.object_type = client->ship_to_place;
    size_t size;
    char *content = you_place_packet_serialization(&you_place, &size);
    send_packet(client, PACKET_YOU_PLACE, content, size);
}

/// @brief Informs everyone whose turn it is and what active player can still do.
void send_everyone_you_go()
{
    struct Client *active = find_client_by_id(game.active_player_id);
    struct YouGoPacket you_go;
    memset(&you_go, 0, sizeof(you_go));
    you_go.player_id = game.active_player_id;
    you_go.turn_number = (uint16_t)game.turn_number;
    you_go.max_turns = (uint16_t)config.max_turns;
    you_go.shots_left = (uint8_t)game.shots_left;
    you_go.seconds_left = (uint8_t)timer_seconds_remaining(&game.turn_timer);
    if (active)
    {
        memcpy(you_go.moves_left, active->moves_left, SHIP_TYPE_COUNT);
    }
    size_t i = 0;
    while (i < MAX_CLIENT_SLOTS)
    {
        if (client_is_active(&clients[i]))
        {
            size_t size;
            char *content = you_go_packet_serialization(&you_go, &size);
            send_packet(&clients[i], PACKET_YOU_GO, content, size);
        }
        i++;
    }
}

// ---------------------------------------------------------------------------
// Clients and registration
// ---------------------------------------------------------------------------

int connected_client_count()
{
    int count = 0;
    size_t i = 0;
    while (i < MAX_CLIENT_SLOTS)
    {
        if (clients[i].in_use && clients[i].connection.socket >= 0)
        {
            count++;
        }
        i++;
    }
    return count;
}

/// @brief Finds smallest player ID that is not used. IDs are 1 byte so they are reused.
uint8_t get_free_player_id()
{
    int id = 1;
    while (id < 256)
    {
        if (!find_client_by_id((uint8_t)id))
        {
            return (uint8_t)id;
        }
        id++;
    }
    return 0;
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
    if (connected_client_count() >= config.max_players)
    {
        return 2;
    }
    size_t i = 0;
    while (i < MAX_CLIENT_SLOTS)
    {
        if (!clients[i].in_use)
        {
            memset(&clients[i], 0, sizeof(struct Client));
            clients[i].in_use = 1;
            connection_init(&clients[i].connection, socket);
            clients[i].player.socket_nr = socket;
            clients[i].player.status = PLAYER_STATUS_NEW;
            clients[i].player.team_id = 0;
            clients[i].connected_at_ms = now_ms();
            return 0;
        }
        i++;
    }
    // Clients too much.
    return 2;
}

/// @brief Checks if client with such username is already registered.
/// @param username Username to check.
/// @return NULL if no player with such username found. Otherwise client with same username.
struct Client *username_exists(char *username)
{
    size_t i = 0;
    while (i < MAX_CLIENT_SLOTS)
    {
        if (clients[i].in_use && clients[i].player.status > PLAYER_STATUS_NEW)
        {
            if (strcmp(username, clients[i].player.username) == 0)
            {
                return &clients[i];
            }
        }
        i++;
    }
    return NULL;
}

char *username_invalid(char *username)
{
    size_t length = strlen(username);
    if (length < MIN_USERNAME || length > MAX_USERNAME)
    {
        return INVALID_USERNAME_LENGTH;
    }
    if (username[0] == ' ' || username[length - 1] == ' ')
    {
        return INVALID_USERNAME_SPACE_AT_END;
    }
    size_t i = 0;
    while (i < length)
    {
        if (!isprint((unsigned char)username[i]))
        {
            return INVALID_USERNAME_CHARACTERS;
        }
        i++;
    }
    return NULL;
}

/// @brief Team with fewest players (used for joining lobby).
uint8_t get_team_for_new_player()
{
    int counts[MAX_TEAMS + 1] = {0};
    size_t i = 0;
    while (i < MAX_CLIENT_SLOTS)
    {
        if (client_is_player(&clients[i]))
        {
            counts[clients[i].player.team_id]++;
        }
        i++;
    }
    uint8_t best = 1;
    uint8_t team = 2;
    while (team <= config.team_count)
    {
        if (counts[team] < counts[best])
        {
            best = team;
        }
        team++;
    }
    return best;
}

void send_end_game(struct Client *client)
{
    size_t size;
    char *content = end_game_packet_serialization(&game.result, &size);
    send_packet(client, PACKET_END_GAME, content, size);
}

void send_ack(struct Client *client, uint8_t player_id, uint8_t team_id, char *message)
{
    struct AckPacket ack_packet;
    ack_packet.player_id = player_id;
    ack_packet.team_id = team_id;
    ack_packet.message = message;
    ack_packet.message_length = message ? (uint8_t)strlen(message) : 0;
    size_t size;
    char *content = ack_packet_serialization(&ack_packet, &size);
    send_packet(client, PACKET_ACK, content, size);
}

void disconnect_client(struct Client *client, const char *reason);
void next_turn();

/// @brief Takes over player slot of disconnected player with same name (reconnect mid-game).
void reclaim_player(struct Client *new_client, struct Client *old_client)
{
    // Connection moves to old slot so that player keeps ID, team and ships.
    old_client->connection = new_client->connection;
    old_client->player.socket_nr = new_client->connection.socket;
    old_client->player.status = PLAYER_STATUS_REGISTERED;
    connection_init(&new_client->connection, -1);
    memset(new_client, 0, sizeof(struct Client));
    connection_init(&new_client->connection, -1);
    send_ack(old_client, old_client->player.id, old_client->player.team_id, "Welcome Back!");
    log_message(LOG_LEVEL_INFO, "Player %s (ID %d) reconnected", old_client->player.username, old_client->player.id);
    announce("%s Reconnected", old_client->player.username);
    if (game.status == GAME_STATUS_SHIP_PLACEMENT && old_client->ship_to_place)
    {
        send_you_place(old_client);
    }
    if (game.status == GAME_STATUS_MAIN_GAME)
    {
        if (game.active_player_id == 0)
        {
            // Game was waiting for someone to come back.
            next_turn();
        }
        else
        {
            send_everyone_you_go();
        }
    }
    if (game.status == GAME_STATUS_GAME_OVER)
    {
        send_end_game(old_client);
    }
    game.state_changed = 1;
}

/// @brief Processes HELLO packet and registers client username (INTR-0000).
/// @return 1 if succesful registration. 0 if registration failed.
int register_client_username(struct Client *client, struct GenericPacket *client_packet)
{
    struct HelloPacket hello_packet;
    if (hello_packet_deserialization(client_packet->content, client_packet->packet_content_size, &hello_packet))
    {
        free(hello_packet.name);
        send_ack(client, 0, 0, "Broken Hello Packet!");
        return 0;
    }
    // Check if username isn't already registered. Or isn't invalid.
    char *is_invalid = username_invalid(hello_packet.name);
    if (!is_invalid && strlen(hello_packet.name) != hello_packet.name_length)
    {
        // Name contains \0 inside. It would look like different name.
        is_invalid = INVALID_USERNAME_CHARACTERS;
    }
    struct Client *same_name = is_invalid ? NULL : username_exists(hello_packet.name);
    int registration_status = 0;
    if (is_invalid)
    {
        // Username invalid.
        send_ack(client, 0, 0, is_invalid);
    }
    else if (same_name && same_name->player.status == PLAYER_STATUS_DISCONNECTED && game.status != GAME_STATUS_LOBBY)
    {
        // Same player came back during game.
        reclaim_player(client, same_name);
        registration_status = 1;
    }
    else if (same_name)
    {
        // Username duplicates.
        send_ack(client, 0, 0, DUPLICATE_USERNAMES);
    }
    else
    {
        // Succesful registration.
        strcpy(client->player.username, hello_packet.name);
        client->player.id = get_free_player_id();
        if (game.status == GAME_STATUS_LOBBY)
        {
            client->player.team_id = get_team_for_new_player();
            client->player.status = PLAYER_STATUS_REGISTERED;
            send_ack(client, client->player.id, client->player.team_id, "Welcome!");
        }
        else
        {
            // Players who join after lobby closes can only watch.
            client->player.team_id = 0;
            client->player.status = PLAYER_STATUS_OBSERVER;
            send_ack(client, client->player.id, 0, "Game Already Started. You Can Watch.");
            if (game.status == GAME_STATUS_GAME_OVER)
            {
                send_end_game(client);
            }
        }
        log_message(LOG_LEVEL_INFO, "Welcome client (%s) ID: %d Team: %d Socket: %d", client->player.username, client->player.id, client->player.team_id, client->connection.socket);
        announce("%s Joined%s", client->player.username, client->player.team_id ? "" : " As Observer");
        registration_status = 1;
        game.state_changed = 1;
    }
    free(hello_packet.name);
    return registration_status;
}

/// @brief Does client registration. Accepts all waiting connections.
/// All actions should be non-blocking here so that main game loop is not affected.
void register_clients()
{
    while (1)
    {
        int client_socket = accept_connection(server_tcp_socket);
        if (client_socket == -2)
        {
            // No client connections waiting.
            return;
        }
        if (client_socket < 0)
        {
            print_failure("Failure while accepting client connection\n");
            return;
        }
        // Set client non-blocking to not halt server.
        set_socket_non_blocking(client_socket);
        switch (add_client_socket_info(client_socket))
        {
        case 0:
            break;
        case 2:
        {
            log_message(LOG_LEVEL_WARNING, "Too much clients connected already! Rejecting socket %d", client_socket);
            struct Client rejected;
            memset(&rejected, 0, sizeof(rejected));
            connection_init(&rejected.connection, client_socket);
            send_ack(&rejected, 0, 0, SERVER_FULL);
            connection_close(&rejected.connection);
            break;
        }
        default:
            print_failure("Impossible failure!\n");
        }
    }
}

// ---------------------------------------------------------------------------
// Game flow
// ---------------------------------------------------------------------------

int random_number()
{
    return rand();
}

struct MapObject *find_ship(uint8_t owner_id, uint8_t object_type)
{
    size_t i = 0;
    while (i < game.ship_count)
    {
        if (game.ships[i].owner_id == owner_id && game.ships[i].object_type == object_type)
        {
            return &game.ships[i];
        }
        i++;
    }
    return NULL;
}

int player_alive_ships(uint8_t player_id)
{
    int count = 0;
    size_t i = 0;
    while (i < game.ship_count)
    {
        if (game.ships[i].owner_id == player_id && !ship_is_sunk(&game.ships[i]))
        {
            count++;
        }
        i++;
    }
    return count;
}

/// @brief Places remaining ships of player randomly (placement time is up or player left).
void auto_place_ships(struct Client *client)
{
    while (client->ship_to_place != 0)
    {
        struct MapObject ship;
        memset(&ship, 0, sizeof(ship));
        ship.object_type = client->ship_to_place;
        ship.team_id = client->player.team_id;
        ship.owner_id = client->player.id;
        if (random_placement(&ship, game.ships, game.ship_count, (uint8_t)config.map_width, (uint8_t)config.map_height, random_number) == RULE_OK)
        {
            game.ships[game.ship_count++] = ship;
        }
        else
        {
            log_message(LOG_LEVEL_WARNING, "No space for %s of %s", get_ship_properties(ship.object_type)->name, client->player.username);
        }
        client->ship_to_place = client->ship_to_place < SHIP_TYPE_COUNT ? client->ship_to_place + 1 : 0;
    }
    game.state_changed = 1;
}

/// @brief Lobby closes. Teams are balanced and players are asked to place ships (LOGC-0006).
void start_ship_placement()
{
    game.status = GAME_STATUS_SHIP_PLACEMENT;
    game.ship_count = 0;
    game.marker_count = 0;
    stop_timer(&game.lobby_timer);
    start_or_reset_timer(&game.placement_timer, config.placement_seconds);
    // Balance teams in order of joining (by player ID).
    int player_number = 0;
    int id = 1;
    while (id < 256)
    {
        struct Client *client = find_client_by_id((uint8_t)id++);
        if (client && client->player.status == PLAYER_STATUS_REGISTERED)
        {
            client->player.team_id = (uint8_t)(player_number++ % config.team_count + 1);
            client->ship_to_place = 1;
            client->hits = 0;
            send_you_place(client);
        }
    }
    print_success("Lobby is closing. Players in game: ");
    printf("%d\n", player_number);
    announce("Lobby Closed! Place Your Ships In Your Team Corner.");
    game.state_changed = 1;
}

void end_game(uint8_t winner_team_id, const char *reason);

void start_main_game()
{
    game.status = GAME_STATUS_MAIN_GAME;
    stop_timer(&game.placement_timer);
    game.turn_number = 0;
    game.current_team = 0;
    memset(game.last_player_of_team, 0, sizeof(game.last_player_of_team));
    memset(game.team_away_since_ms, 0, sizeof(game.team_away_since_ms));
    game.active_player_id = 1; // Anything not 0 so that first turn is announced normally.
    announce("All Ships Placed. Battle Begins!");
    next_turn();
}

/// @brief Team with most undamaged ship squares (used when game can't be decided by sinking).
/// @return Team ID or 0 if teams are equal.
uint8_t team_with_most_healthy_squares()
{
    int best_score = -1, best_count = 0;
    uint8_t best_team = 0, team = 1;
    while (team <= config.team_count)
    {
        int score = count_healthy_segments(game.ships, game.ship_count, team);
        if (score > best_score)
        {
            best_score = score;
            best_team = team;
            best_count = 1;
        }
        else if (score == best_score)
        {
            best_count++;
        }
        team++;
    }
    return best_count == 1 ? best_team : 0;
}

/// @brief Can ship move at all (afloat, has speed and isn't damaged).
int ship_can_move(const struct MapObject *ship)
{
    return !ship_is_sunk(ship) && !ship_is_damaged(ship) && get_ship_properties(ship->object_type)->speed > 0;
}

/// @brief Can any ship ever damage an enemy again. False when no ship afloat can shoot, or when
/// nothing can move and no ship has an enemy in range (then nothing on map can ever change).
int anyone_can_still_fight()
{
    int any_shooter = 0, any_moving = 0;
    size_t i = 0;
    while (i < game.ship_count)
    {
        struct MapObject *ship = &game.ships[i++];
        if (!ship_is_sunk(ship) && get_ship_properties(ship->object_type)->range != 0)
        {
            any_shooter = 1;
        }
        any_moving |= ship_can_move(ship);
    }
    if (!any_shooter)
    {
        return 0;
    }
    if (any_moving)
    {
        // Shooter can sail into range or enemy can sail into shooter's range.
        return 1;
    }
    // Nothing moves: fight goes on only if some ship already has an enemy in range.
    i = 0;
    while (i < game.ship_count)
    {
        struct MapObject *shooter = &game.ships[i++];
        if (ship_is_sunk(shooter) || get_ship_properties(shooter->object_type)->range == 0)
        {
            continue;
        }
        size_t j = 0;
        while (j < game.ship_count)
        {
            struct MapObject *enemy = &game.ships[j++];
            if (enemy->team_id == shooter->team_id || ship_is_sunk(enemy))
            {
                continue;
            }
            int xs[MAX_SHIP_LENGTH], ys[MAX_SHIP_LENGTH];
            int length = ship_squares(enemy, xs, ys), segment = 0;
            while (segment < length)
            {
                if (!(enemy->info & (1 << segment)) &&
                    validate_shot(shooter, xs[segment], ys[segment], (uint8_t)config.map_width, (uint8_t)config.map_height) == RULE_OK)
                {
                    return 1;
                }
                segment++;
            }
        }
    }
    return 0;
}

/// @brief Checks if anyone has won (LOGC-0018).
/// @return 1 if game ended.
int check_game_end()
{
    if (game.status != GAME_STATUS_MAIN_GAME)
    {
        return 0;
    }
    int teams_alive = 0, teams_playing = 0;
    uint8_t last_alive = 0, last_playing = 0;
    uint8_t team = 1;
    while (team <= config.team_count)
    {
        if (count_alive_ships(game.ships, game.ship_count, team) > 0)
        {
            teams_alive++;
            last_alive = team;
            // Does team have anyone connected that can act. Short disconnects are forgiven.
            int connected = 0;
            size_t i = 0;
            while (i < MAX_CLIENT_SLOTS && !connected)
            {
                struct Client *client = &clients[i++];
                connected = client->in_use && client->player.status == PLAYER_STATUS_REGISTERED && client->player.team_id == team && player_alive_ships(client->player.id);
            }
            if (connected)
            {
                game.team_away_since_ms[team] = 0;
            }
            else if (game.team_away_since_ms[team] == 0)
            {
                game.team_away_since_ms[team] = now_ms();
            }
            if (connected || now_ms() - game.team_away_since_ms[team] < (int64_t)config.reconnect_seconds * 1000)
            {
                teams_playing++;
                last_playing = team;
            }
        }
        team++;
    }
    if (teams_alive <= 1)
    {
        end_game(last_alive, teams_alive ? "Only One Team Has Ships Left!" : "All Ships Are Sunk!");
        return 1;
    }
    if (teams_playing == 1)
    {
        end_game(last_playing, "Other Teams Left The Game!");
        return 1;
    }
    if (teams_playing == 0)
    {
        end_game(0, "Everyone Left The Game!");
        return 1;
    }
    if (!anyone_can_still_fight())
    {
        // For example only Fast Boats are left, or damaged ships that can't move are out of each other's range.
        end_game(team_with_most_healthy_squares(), "No Ship Can Reach An Enemy Anymore!");
        return 1;
    }
    if (game.turn_number >= config.max_turns)
    {
        end_game(team_with_most_healthy_squares(), "Turn Limit Reached!");
        return 1;
    }
    return 0;
}

/// @brief Can player take a turn now.
int player_can_act(struct Client *client)
{
    return client && client->player.status == PLAYER_STATUS_REGISTERED && client->player.team_id != 0 && player_alive_ships(client->player.id) > 0;
}

/// @brief Finds next player of team after last one who acted.
struct Client *next_player_of_team(uint8_t team)
{
    uint8_t last = game.last_player_of_team[team];
    int step = 1;
    while (step <= 256)
    {
        uint8_t id = (uint8_t)((last + step) % 256);
        struct Client *client = find_client_by_id(id);
        if (client && client->player.team_id == team && player_can_act(client))
        {
            return client;
        }
        step++;
    }
    return NULL;
}

void begin_turn(struct Client *client)
{
    game.turn_number++;
    game.active_player_id = client->player.id;
    game.current_team = client->player.team_id;
    game.last_player_of_team[client->player.team_id] = client->player.id;
    game.shots_left = config.shots_per_turn;
    uint8_t type = 1;
    while (type <= SHIP_TYPE_COUNT)
    {
        struct MapObject *ship = find_ship(client->player.id, type);
        int can_move = ship && !ship_is_sunk(ship) && !ship_is_damaged(ship);
        client->moves_left[type - 1] = can_move ? get_ship_properties(type)->speed : 0;
        type++;
    }
    if (config.turn_seconds > 0)
    {
        start_or_reset_timer(&game.turn_timer, config.turn_seconds);
    }
    announce("Turn %d/%d: %s (Team %d)", game.turn_number, config.max_turns, client->player.username, client->player.team_id);
    send_everyone_you_go();
    game.state_changed = 1;
}

void next_turn()
{
    if (check_game_end())
    {
        return;
    }
    // Next team after current one that has someone who can act.
    int step = 1;
    while (step <= config.team_count)
    {
        uint8_t team = (uint8_t)((game.current_team + step - 1) % config.team_count + 1);
        struct Client *client = next_player_of_team(team);
        if (client)
        {
            begin_turn(client);
            return;
        }
        step++;
    }
    // Everyone who could act is disconnected. Wait for them (check_game_end decides when waiting is over).
    if (game.active_player_id != 0)
    {
        announce("Waiting For Players To Reconnect...");
        game.active_player_id = 0;
        stop_timer(&game.turn_timer);
        send_everyone_you_go();
        game.state_changed = 1;
    }
}

void end_game(uint8_t winner_team_id, const char *reason)
{
    game.status = GAME_STATUS_GAME_OVER;
    game.active_player_id = 0;
    stop_timer(&game.turn_timer);
    start_or_reset_timer(&game.post_game_timer, config.post_game_seconds);
    // Winner player is the one from winning team with most hits (nobody if team didn't hit anything).
    struct EndGamePacket end_game_packet;
    end_game_packet.winner_team_id = winner_team_id;
    end_game_packet.winner_id = 0;
    int best_hits = 0;
    size_t i = 0;
    while (i < MAX_CLIENT_SLOTS)
    {
        struct Client *client = &clients[i++];
        if (winner_team_id && client_is_player(client) && client->player.team_id == winner_team_id && client->hits > best_hits)
        {
            best_hits = client->hits;
            end_game_packet.winner_id = client->player.id;
        }
    }
    game.result = end_game_packet;
    struct Client *winner = find_client_by_id(end_game_packet.winner_id);
    if (winner_team_id)
    {
        if (winner)
        {
            announce("%s Team %d Wins! Best Player: %s", reason, winner_team_id, winner->player.username);
        }
        else
        {
            announce("%s Team %d Wins!", reason, winner_team_id);
        }
    }
    else
    {
        announce("%s Nobody Wins This Time.", reason);
    }
    i = 0;
    while (i < MAX_CLIENT_SLOTS)
    {
        if (client_is_active(&clients[i]))
        {
            size_t size;
            char *content = end_game_packet_serialization(&end_game_packet, &size);
            send_packet(&clients[i], PACKET_END_GAME, content, size);
        }
        i++;
    }
    game.state_changed = 1;
}

/// @brief After game ends everyone goes back to lobby and can play again.
void reset_to_lobby()
{
    game.status = GAME_STATUS_LOBBY;
    game.ship_count = 0;
    game.marker_count = 0;
    game.active_player_id = 0;
    game.turn_number = 0;
    stop_timer(&game.post_game_timer);
    size_t i = 0;
    while (i < MAX_CLIENT_SLOTS)
    {
        struct Client *client = &clients[i++];
        if (!client->in_use)
        {
            continue;
        }
        if (client->player.status == PLAYER_STATUS_DISCONNECTED)
        {
            // Player didn't come back. Free the slot.
            memset(client, 0, sizeof(struct Client));
            connection_init(&client->connection, -1);
            continue;
        }
        if (client->player.status == PLAYER_STATUS_REGISTERED || client->player.status == PLAYER_STATUS_OBSERVER)
        {
            // Observers can play next game.
            client->player.status = PLAYER_STATUS_REGISTERED;
            client->player.team_id = 0;
            client->ship_to_place = 0;
        }
    }
    i = 0;
    while (i < MAX_CLIENT_SLOTS)
    {
        struct Client *client = &clients[i++];
        if (client->in_use && client->player.status == PLAYER_STATUS_REGISTERED)
        {
            client->player.team_id = get_team_for_new_player();
        }
    }
    game.registered_count = -1; // Forces lobby timer check.
    announce("New Game Lobby Is Open!");
    game.state_changed = 1;
}

/// @brief Adds shot marker that only shooter team sees. Old markers are forgotten.
void add_shot_marker(uint8_t team_id, int x, int y, int hit)
{
    int team_markers = 0;
    size_t i = 0, oldest = MAX_MARKERS;
    while (i < game.marker_count)
    {
        if (game.markers[i].team_id == team_id)
        {
            if (oldest == MAX_MARKERS)
            {
                oldest = i;
            }
            team_markers++;
        }
        i++;
    }
    if (team_markers >= SHOT_MARKERS_PER_TEAM && oldest < game.marker_count)
    {
        memmove(&game.markers[oldest], &game.markers[oldest + 1], (game.marker_count - oldest - 1) * sizeof(struct MapObject));
        game.marker_count--;
    }
    if (game.marker_count >= MAX_MARKERS)
    {
        return;
    }
    struct MapObject *marker = &game.markers[game.marker_count++];
    memset(marker, 0, sizeof(struct MapObject));
    marker->object_type = hit ? OBJECT_HIT_SQUARE : OBJECT_MISSED_HIT;
    marker->x = (uint8_t)x;
    marker->y = (uint8_t)y;
    marker->team_id = team_id;
}

int any_action_left(struct Client *client)
{
    if (game.shots_left > 0)
    {
        return 1;
    }
    size_t i = 0;
    while (i < SHIP_TYPE_COUNT)
    {
        if (client->moves_left[i++])
        {
            return 1;
        }
    }
    return 0;
}

/// @brief Validates and performs player action (IGo packet).
void process_action(struct Client *client, struct IGoPacket *action)
{
    if (game.status != GAME_STATUS_MAIN_GAME || game.active_player_id != client->player.id)
    {
        tell(client, "%s", rule_text(RULE_NOT_YOUR_TURN));
        return;
    }
    if (action->action == ACTION_END_TURN)
    {
        log_message(LOG_LEVEL_DEBUG, "%s: end turn", client->player.username);
        next_turn();
        return;
    }
    struct MapObject *ship = find_ship(client->player.id, action->object_type);
    if (!ship || ship_is_sunk(ship))
    {
        tell(client, "%s", rule_text(ship ? RULE_SHIP_SUNK : RULE_NO_SUCH_SHIP));
        return;
    }
    const struct ShipProperties *properties = get_ship_properties(ship->object_type);
    log_message(LOG_LEVEL_DEBUG, "%s: %s with %s (x %d, y %d, direction %d)", client->player.username, action->action == ACTION_MOVE ? "move" : "shoot",
                properties->name, action->x, action->y, action->direction);
    int result = RULE_OK;
    if (action->action == ACTION_MOVE)
    {
        if (properties->speed == 0)
        {
            result = RULE_SHIP_CANNOT_MOVE;
        }
        else if (ship_is_damaged(ship))
        {
            result = RULE_SHIP_DAMAGED;
        }
        else if (client->moves_left[ship->object_type - 1] == 0)
        {
            result = RULE_NO_MOVES_LEFT;
        }
        else
        {
            result = move_ship(ship, action->direction, game.ships, game.ship_count, (uint8_t)config.map_width, (uint8_t)config.map_height);
        }
        if (result == RULE_OK)
        {
            client->moves_left[ship->object_type - 1]--;
        }
    }
    else if (action->action == ACTION_SHOOT)
    {
        if (game.shots_left <= 0)
        {
            result = RULE_NO_SHOTS_LEFT;
        }
        else
        {
            result = validate_shot(ship, action->x, action->y, (uint8_t)config.map_width, (uint8_t)config.map_height);
        }
        if (result == RULE_OK && team_ship_at(game.ships, game.ship_count, client->player.team_id, action->x, action->y))
        {
            result = RULE_FRIENDLY_FIRE;
        }
        if (result == RULE_OK)
        {
            game.shots_left--;
            int hit_index;
            int shot = apply_shot(game.ships, game.ship_count, action->x, action->y, &hit_index);
            add_shot_marker(client->player.team_id, action->x, action->y, shot != SHOT_MISS);
            if (shot == SHOT_MISS)
            {
                tell(client, "Miss At (%d, %d).", action->x, action->y);
            }
            else if (shot == SHOT_ALREADY_HIT)
            {
                tell(client, "That Part Was Already Hit.");
            }
            else
            {
                struct MapObject *target = &game.ships[hit_index];
                struct Client *victim = find_client_by_id(target->owner_id);
                client->hits++;
                announce("%s %s %s's %s!", client->player.username, shot == SHOT_SUNK ? "Sunk" : "Hit",
                         victim ? victim->player.username : "Unknown", get_ship_properties(target->object_type)->name);
                game.state_changed = 1;
                if (check_game_end())
                {
                    return;
                }
            }
        }
    }
    else
    {
        tell(client, "Unknown Action.");
        return;
    }
    if (result != RULE_OK)
    {
        log_message(LOG_LEVEL_DEBUG, "%s: action rejected: %s", client->player.username, rule_text(result));
        tell(client, "%s", rule_text(result));
        return;
    }
    game.state_changed = 1;
    if (!any_action_left(client))
    {
        // Nothing left to do. Don't make everyone wait.
        log_message(LOG_LEVEL_DEBUG, "%s: nothing left to do, turn ends", client->player.username);
        next_turn();
        return;
    }
    send_everyone_you_go();
}

/// @brief Validates ship placement (IPlace packet).
void process_placement(struct Client *client, struct IPlacePacket *i_place_packet)
{
    if (game.status != GAME_STATUS_SHIP_PLACEMENT || client->ship_to_place == 0)
    {
        tell(client, "No Ship To Place Now.");
        return;
    }
    struct MapObject ship = i_place_packet->object;
    // Never trust client with team, owner or damage.
    ship.team_id = client->player.team_id;
    ship.owner_id = client->player.id;
    ship.info = 0;
    int result = ship.object_type == client->ship_to_place ? RULE_OK : RULE_INVALID_SHIP_TYPE;
    if (result == RULE_OK)
    {
        result = validate_placement(&ship, game.ships, game.ship_count, (uint8_t)config.map_width, (uint8_t)config.map_height);
    }
    if (result != RULE_OK)
    {
        tell(client, "%s", rule_text(result));
        send_you_place(client);
        return;
    }
    game.ships[game.ship_count++] = ship;
    log_message(LOG_LEVEL_DEBUG, "%s placed %s at X:%d Y:%d R:%d", client->player.username, get_ship_properties(ship.object_type)->name, ship.x, ship.y, ship.rotation);
    client->ship_to_place = client->ship_to_place < SHIP_TYPE_COUNT ? client->ship_to_place + 1 : 0;
    if (client->ship_to_place)
    {
        send_you_place(client);
    }
    else
    {
        tell(client, "All Ships Placed. Waiting On Others...");
    }
    game.state_changed = 1;
}

/// @brief Player chat (INTR-0002 - INTR-0005).
void process_chat(struct Client *client, struct MessagePacket *message)
{
    if (message->message_type != MESSAGE_TYPE_PLAYER || message->message_length == 0)
    {
        return;
    }
    // Only printable characters. Never trust the network.
    size_t i = 0;
    while (i < message->message_length)
    {
        if (!isprint((unsigned char)message->message[i]))
        {
            message->message[i] = '?';
        }
        i++;
    }
    struct Client *receiver = message->receiver_id ? find_client_by_id(message->receiver_id) : NULL;
    if (message->receiver_id && (!receiver || !client_is_active(receiver)))
    {
        tell(client, "That Player Is Not Online.");
        return;
    }
    log_message(LOG_LEVEL_DEBUG, "Chat %s -> %s: %s", client->player.username, receiver ? receiver->player.username : "All", message->message);
    if (receiver)
    {
        send_message(receiver, MESSAGE_TYPE_PLAYER, client->player.id, receiver->player.id, message->message);
        if (receiver != client)
        {
            send_message(client, MESSAGE_TYPE_PLAYER, client->player.id, receiver->player.id, message->message);
        }
        return;
    }
    i = 0;
    while (i < MAX_CLIENT_SLOTS)
    {
        if (client_is_active(&clients[i]))
        {
            send_message(&clients[i], MESSAGE_TYPE_PLAYER, client->player.id, 0, message->message);
        }
        i++;
    }
}

void disconnect_client(struct Client *client, const char *reason)
{
    char username[MAX_USERNAME + 1];
    strcpy(username, client->player.username);
    log_message(LOG_LEVEL_WARNING, "Client disconnected: %s (ID %d, socket %d) - %s", client->player.username[0] ? client->player.username : "?",
                client->player.id, client->connection.socket, reason);
    connection_close(&client->connection);
    client->player.socket_nr = -1;
    int was_registered = client->player.status == PLAYER_STATUS_REGISTERED || client->player.status == PLAYER_STATUS_OBSERVER;
    int keep_slot = client->player.status == PLAYER_STATUS_REGISTERED && client->player.team_id != 0 && game.status != GAME_STATUS_LOBBY;
    if (keep_slot)
    {
        // Player can come back with same name. Ships stay on map.
        client->player.status = PLAYER_STATUS_DISCONNECTED;
        if (game.status == GAME_STATUS_SHIP_PLACEMENT)
        {
            auto_place_ships(client);
        }
    }
    else
    {
        memset(client, 0, sizeof(struct Client));
        connection_init(&client->connection, -1);
    }
    if (was_registered)
    {
        announce("%s Left The Game", username);
    }
    game.state_changed = 1;
    if (game.status == GAME_STATUS_MAIN_GAME && keep_slot)
    {
        if (game.active_player_id == client->player.id)
        {
            next_turn();
        }
        else
        {
            check_game_end();
        }
    }
}

/// @brief Processes one packet from client.
/// @return 0 on success, 1 if packet was not expected.
int process_client_packet(struct Client *client, struct GenericPacket *client_packet)
{
    if (client->player.status == PLAYER_STATUS_NEW)
    {
        if (client_packet->packet_type != PACKET_HELLO)
        {
            send_ack(client, 0, 0, "Send HELLO First!");
            return 1;
        }
        register_client_username(client, client_packet);
        return 0;
    }
    switch (client_packet->packet_type)
    {
    case PACKET_HELLO:
        log_message(LOG_LEVEL_WARNING, "Client %s sent HELLO again. Ignoring.", client->player.username);
        return 1;
    case PACKET_MESSAGE:
    {
        struct MessagePacket message;
        if (message_packet_deserialization(client_packet->content, client_packet->packet_content_size, &message) == 0)
        {
            process_chat(client, &message);
        }
        free(message.message);
        return 0;
    }
    case PACKET_I_PLACE:
    {
        struct IPlacePacket i_place_packet;
        if (i_place_packet_deserialization(client_packet->content, client_packet->packet_content_size, &i_place_packet) == 0)
        {
            process_placement(client, &i_place_packet);
        }
        return 0;
    }
    case PACKET_I_GO:
    {
        struct IGoPacket i_go_packet;
        if (i_go_packet_deserialization(client_packet->content, client_packet->packet_content_size, &i_go_packet) == 0)
        {
            process_action(client, &i_go_packet);
        }
        return 0;
    }
    default:
        log_message(LOG_LEVEL_WARNING, "Unknown packet type %d received from client %s", client_packet->packet_type, client->player.username);
        return 1;
    }
}

/// @brief Reads data from all clients and processes complete packets.
void process_incoming_packets()
{
    size_t i = 0;
    while (i < MAX_CLIENT_SLOTS)
    {
        struct Client *client = &clients[i++];
        if (!client->in_use || client->connection.socket < 0)
        {
            continue;
        }
        int read_status = connection_read(&client->connection);
        struct GenericPacket *packet;
        // Process everything that has arrived (also when peer has just closed connection).
        while (client->in_use && client->connection.socket >= 0 && (packet = connection_next_packet(&client->connection)) != NULL)
        {
            process_client_packet(client, packet);
            free_generic_packet(packet);
        }
        if (!client->in_use || client->connection.socket < 0)
        {
            continue;
        }
        if (read_status != CONNECTION_OK || client->connection.state != CONNECTION_OK)
        {
            disconnect_client(client, client->connection.state == CONNECTION_PROTOCOL_ERROR ? "protocol error" : "connection closed");
        }
        else if (client->player.status == PLAYER_STATUS_NEW && now_ms() - client->connected_at_ms > REGISTRATION_TIMEOUT_SECONDS * 1000)
        {
            disconnect_client(client, "did not register in time");
        }
    }
}

int count_registered_players()
{
    int count = 0;
    size_t i = 0;
    while (i < MAX_CLIENT_SLOTS)
    {
        if (clients[i].in_use && clients[i].player.status == PLAYER_STATUS_REGISTERED)
        {
            count++;
        }
        i++;
    }
    return count;
}

/// @brief Moves game to next stage when it is time (LOGC-0006, LOGC-0007, LOGC-0011).
void update_game_status()
{
    switch (game.status)
    {
    case GAME_STATUS_LOBBY:
    {
        int registered = count_registered_players();
        player_count_changed = registered != game.registered_count;
        game.registered_count = registered;
        // if player count changed, that means it is now either
        // 1) below the minimum threshold which means the timer is not needed
        // or
        // 2) we need to reset the timer due to players joining / disconnecting
        if (player_count_changed)
        {
            if (registered < config.min_players)
            {
                stop_timer(&game.lobby_timer);
            }
            else
            {
                start_or_reset_timer(&game.lobby_timer, config.lobby_wait_seconds);
                announce("Game Starts In %d Seconds. New Player Resets Timer.", config.lobby_wait_seconds);
            }
        }
        // Lobby closes when time is up or server is full.
        if (registered >= config.min_players && (timer_has_finished_running(&game.lobby_timer) || registered >= config.max_players))
        {
            start_ship_placement();
        }
        break;
    }
    case GAME_STATUS_SHIP_PLACEMENT:
    {
        int waiting = 0;
        size_t i = 0;
        while (i < MAX_CLIENT_SLOTS)
        {
            struct Client *client = &clients[i++];
            if (client_is_player(client) && client->ship_to_place)
            {
                if (timer_has_finished_running(&game.placement_timer))
                {
                    tell(client, "Time Is Up! Remaining Ships Placed Randomly.");
                    auto_place_ships(client);
                }
                else
                {
                    waiting++;
                }
            }
        }
        if (!waiting)
        {
            start_main_game();
        }
        break;
    }
    case GAME_STATUS_MAIN_GAME:
        if (check_game_end())
        {
            break;
        }
        if (game.active_player_id == 0)
        {
            // Waiting for someone to reconnect.
            next_turn();
        }
        else if (timer_has_finished_running(&game.turn_timer))
        {
            struct Client *active = find_client_by_id(game.active_player_id);
            if (active)
            {
                tell(active, "Time Is Up!");
            }
            next_turn();
        }
        break;
    case GAME_STATUS_GAME_OVER:
        if (timer_has_finished_running(&game.post_game_timer))
        {
            reset_to_lobby();
        }
        break;
    }
}

char *get_state_message(struct Client *client)
{
    if (client->player.status == PLAYER_STATUS_OBSERVER)
    {
        return STATE_MESSAGE_OBSERVER;
    }
    switch (game.status)
    {
    case GAME_STATUS_LOBBY:
        return STATE_MESSAGE_LOBBY;
    case GAME_STATUS_SHIP_PLACEMENT:
        return client->ship_to_place ? STATE_MESSAGE_SHIP_PLACEMENT : STATE_MESSAGE_WAITING_ON_PLAYERS;
    case GAME_STATUS_MAIN_GAME:
        return game.active_player_id == client->player.id ? STATE_MESSAGE_YOUR_TURN : STATE_MESSAGE_WAITING_ON_PLAYERS;
    case GAME_STATUS_GAME_OVER:
        return STATE_MESSAGE_GAME_OVER;
    default:
        return "GAME BROKE!";
    }
}

/// @brief Builds state that client is allowed to see (BIC-73). Enemy ships are visible only in view range.
void build_client_state(struct Client *client, struct StatePacket *state, struct MapObject *objects, struct Player *players)
{
    uint8_t team = client->player.status == PLAYER_STATUS_OBSERVER ? 0 : client->player.team_id;
    state->map_width = (uint8_t)config.map_width;
    state->map_height = (uint8_t)config.map_height;
    state->status = game.status;
    state->object_count = 0;
    state->map_objects = objects;
    size_t i = 0;
    while (i < game.ship_count && state->object_count < 255)
    {
        // During game over everyone sees all ships.
        if (game.status == GAME_STATUS_GAME_OVER || team_sees_object(team, &game.ships[i], game.ships, game.ship_count))
        {
            objects[state->object_count++] = game.ships[i];
        }
        i++;
    }
    i = 0;
    while (i < game.marker_count && state->object_count < 255)
    {
        if (team != 0 && game.markers[i].team_id == team)
        {
            objects[state->object_count++] = game.markers[i];
        }
        i++;
    }
    state->player_count = 0;
    state->players = players;
    i = 0;
    while (i < MAX_CLIENT_SLOTS)
    {
        if (clients[i].in_use && clients[i].player.status > PLAYER_STATUS_NEW)
        {
            players[state->player_count] = clients[i].player;
            players[state->player_count++].socket_nr = -1; // Not needed for client.
        }
        i++;
    }
}

/// @brief Builds status text shown in status area (ship counts and what to do).
void build_status_text(struct Client *client, char *text, size_t size)
{
    char *state_message = get_state_message(client);
    if (game.status == GAME_STATUS_LOBBY)
    {
        snprintf(text, size, "%s\n\nPlayers: %d (Need %d)", state_message, count_registered_players(), config.min_players);
        return;
    }
    uint8_t team = client->player.team_id;
    int team_ships = team ? count_alive_ships(game.ships, game.ship_count, team) : 0, enemy_ships = 0;
    uint8_t other = 1;
    while (other <= config.team_count)
    {
        if (other != team)
        {
            enemy_ships += count_alive_ships(game.ships, game.ship_count, other);
        }
        other++;
    }
    if (team)
    {
        snprintf(text, size, "%s\n\nTeam Ships: %d\nEnemy Ships: %d", state_message, team_ships, enemy_ships);
    }
    else
    {
        snprintf(text, size, "%s\n\nShips Afloat: %d", state_message, enemy_ships);
    }
}

void build_timer_text(char *text, size_t size)
{
    text[0] = '\0';
    switch (game.status)
    {
    case GAME_STATUS_LOBBY:
        if (game.lobby_timer.is_running)
        {
            snprintf(text, size, "Game Starts In %ds", timer_seconds_remaining(&game.lobby_timer));
        }
        break;
    case GAME_STATUS_SHIP_PLACEMENT:
        snprintf(text, size, "Placement Ends In %ds", timer_seconds_remaining(&game.placement_timer));
        break;
    case GAME_STATUS_MAIN_GAME:
        if (game.turn_timer.is_running)
        {
            snprintf(text, size, "Turn %d/%d Ends In %ds", game.turn_number, config.max_turns, timer_seconds_remaining(&game.turn_timer));
        }
        else
        {
            snprintf(text, size, "Turn %d/%d", game.turn_number, config.max_turns);
        }
        break;
    case GAME_STATUS_GAME_OVER:
        snprintf(text, size, "New Game In %ds", timer_seconds_remaining(&game.post_game_timer));
        break;
    }
}

/// @brief Update active and connected clients with game state.
void send_everyone_state()
{
    int64_t now = now_ms();
    int regular_update = now - game.last_state_sent_ms >= STATE_UPDATE_INTERVAL_MS;
    int quick_update = game.state_changed && now - game.last_state_sent_ms >= MIN_STATE_INTERVAL_MS;
    if (!regular_update && !quick_update)
    {
        return;
    }
    game.last_state_sent_ms = now;
    game.state_changed = 0;
    static struct MapObject objects[MAX_SHIPS + MAX_MARKERS];
    static struct Player players[MAX_CLIENT_SLOTS];
    char status_text[200], timer_text[100];
    build_timer_text(timer_text, sizeof(timer_text));
    size_t i = 0;
    while (i < MAX_CLIENT_SLOTS)
    {
        struct Client *client = &clients[i++];
        if (!client_is_active(client))
        {
            continue;
        }
        struct StatePacket state;
        build_client_state(client, &state, objects, players);
        size_t size;
        char *content = state_packet_serialization(&state, &size);
        send_packet(client, PACKET_STATE, content, size);
        // Status and timer go together with state so that everything on screen matches.
        build_status_text(client, status_text, sizeof(status_text));
        send_message(client, MESSAGE_TYPE_STATUS, 0, client->player.id, status_text);
        send_message(client, MESSAGE_TYPE_TIMER, 0, client->player.id, timer_text);
    }
}

/// @brief Waits until any socket has data (or short timeout) so that server doesn't use CPU for nothing.
void wait_for_activity()
{
    struct pollfd poll_list[MAX_CLIENT_SLOTS + 1];
    nfds_t count = 0;
    poll_list[count].fd = server_tcp_socket;
    poll_list[count++].events = POLLIN;
    size_t i = 0;
    while (i < MAX_CLIENT_SLOTS)
    {
        struct Client *client = &clients[i++];
        if (client->in_use && client->connection.socket >= 0)
        {
            poll_list[count].fd = client->connection.socket;
            poll_list[count++].events = POLLIN | (connection_has_pending_send(&client->connection) ? POLLOUT : 0);
        }
    }
    poll(poll_list, count, LOOP_POLL_MS);
}

/// @brief Sends data that couldn't be sent immediately.
void flush_clients()
{
    size_t i = 0;
    while (i < MAX_CLIENT_SLOTS)
    {
        struct Client *client = &clients[i++];
        if (client->in_use && client->connection.socket >= 0 && connection_has_pending_send(&client->connection))
        {
            connection_flush(&client->connection);
        }
    }
}

/// @brief Do all required actions within loop.
/// @return
int serverloop()
{
    log_message(LOG_LEVEL_INFO, "Starting server loop!");
    game.registered_count = -1;
    while (gameloop)
    {
        gameloop_iteration++;
        wait_for_activity();
        register_clients();
        process_incoming_packets();
        update_game_status();
        send_everyone_state();
        flush_clients();
    }
    return 0;
}

/// @brief Server cleanup actions. Memory cleaning, socket disconnections.
void server_cleanup()
{
    log_message(LOG_LEVEL_INFO, "Cleanup start!");
    size_t i = 0;
    while (i < MAX_CLIENT_SLOTS)
    {
        if (clients[i].in_use && clients[i].connection.socket >= 0)
        {
            connection_flush(&clients[i].connection);
        }
        connection_close(&clients[i].connection);
        i++;
    }
    log_message(LOG_LEVEL_INFO, "Cleanup end!");
}

int main(int argc, char **argv)
{
    printf("Welcome to Battleships Server!\n");
    printf("Version: %s\n", APP_VERSION);
    config_defaults(&config);
    int parse_result = config_parse_args(&config, argc, argv, APP_VERSION);
    if (parse_result != 0)
    {
        return parse_result > 0 ? 0 : 1;
    }
    if (config_validate(&config))
    {
        return 1;
    }
    set_log_level(config.verbose ? LOG_LEVEL_DEBUG : LOG_LEVEL_INFO);
    srand((unsigned int)now_ms());
    startup_server();

    // Gameloop can start here.
    serverloop();

    server_cleanup();
    close_socket(server_tcp_socket);
    log_message(LOG_LEVEL_INFO, "Server stopped.");
    return 0;
}
