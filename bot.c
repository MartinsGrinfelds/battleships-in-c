#include <stdio.h>
#include <stdlib.h> // exit(), size_t, malloc
#include <string.h> // strlen()
#include <unistd.h> // usleep()
#include <signal.h> // signal()
#include <poll.h>   // poll()
#include "packets/connection.h"
#include "packets/shared_packets.h"
#include "game/game_logic.h"
#include "graphical/text_formatter.h" // log_message()
#include "utils/timer.h"

// Computer player without graphics. Can fill a game when there are not enough people
// and is used by tests to play complete games against the server.

#ifdef BATTLESHIPS_VERSION
char *APP_VERSION = BATTLESHIPS_VERSION;
#else
char *APP_VERSION = "UNDEFINED!";
#endif

#define MAX_ACTIONS_PER_TURN 30
#define ACTION_RETRY_MS 1500 // If server doesn't answer action, try something else.

/// @brief Bot settings.
struct BotOptions
{
    const char *host;
    int port;
    char name[MAX_USERNAME + 1];
    int games; // Games to play before exit. 0 - forever.
    int think_ms; // Delay before each action so that people can follow.
    int chaos; // Testing: send data in tiny pieces and disconnect once during game.
    int quiet;
};

struct BotOptions options = {"127.0.0.1", 12345, "Bot", 1, 400, 0, 0};
struct Connection connection;
struct StatePacket live_state;
uint8_t player_id = 0, team_id = 0;
struct YouGoPacket current_turn;
int my_turn = 0, actions_this_turn = 0, last_turn_number = -1;
int64_t next_action_ms = 0, last_action_ms = 0;
int waiting_answer = 0;
uint8_t ship_to_place = 0;
int64_t place_at_ms = 0;
int games_played = 0, wins = 0;
int chaos_disconnected = 0;
uint8_t my_shots[256 * 256]; // Squares this bot shot at in current sweep (index y * 256 + x).
volatile sig_atomic_t running = 1;

void handle_stop_signal(int signal_number)
{
    (void)signal_number;
    running = 0;
}

int random_number()
{
    return rand();
}

void say(const char *format, const char *text)
{
    if (!options.quiet)
    {
        log_message(LOG_LEVEL_INFO, format, options.name, text);
    }
}

void send_packet(uint8_t packet_type, char *content, size_t size)
{
    connection_send(&connection, packet_type, content, size);
    free(content);
}

/// @brief Connects and registers. Tries other names if name is taken.
/// @return 0 on success.
int connect_and_register()
{
    int attempt = 0;
    while (running && attempt < 50)
    {
        int socket = connect_to_host(options.host, (uint16_t)options.port, 3000);
        if (socket < 0)
        {
            attempt++;
            usleep(200 * 1000);
            continue;
        }
        connection_init(&connection, socket);
        if (options.chaos)
        {
            connection.send_chunk_limit = 1 + rand() % 3;
        }
        struct HelloPacket hello;
        hello.name = options.name;
        hello.name_length = (uint8_t)strlen(options.name);
        size_t size;
        char *content = hello_packet_serialization(&hello, &size);
        send_packet(PACKET_HELLO, content, size);
        // Wait for ACK.
        int64_t deadline = now_ms() + 10000;
        while (running && now_ms() < deadline)
        {
            connection_flush(&connection);
            struct pollfd poll_info = {.fd = socket, .events = POLLIN};
            poll(&poll_info, 1, 5);
            if (connection_read(&connection) != CONNECTION_OK && connection.receive_length == 0)
            {
                break;
            }
            struct GenericPacket *packet = connection_next_packet(&connection);
            if (!packet)
            {
                continue;
            }
            if (packet->packet_type != PACKET_ACK)
            {
                free_generic_packet(packet);
                continue;
            }
            struct AckPacket ack;
            ack_packet_deserialization(packet->content, packet->packet_content_size, &ack);
            free_generic_packet(packet);
            if (ack.player_id)
            {
                player_id = ack.player_id;
                team_id = ack.team_id;
                if (!options.quiet)
                {
                    log_message(LOG_LEVEL_INFO, "[%s] Registered with ID %d team %d: %s", options.name, player_id, team_id, ack.message);
                }
                free(ack.message);
                return 0;
            }
            if (!options.quiet)
            {
                log_message(LOG_LEVEL_WARNING, "[%s] Registration failed: %s", options.name, ack.message);
            }
            free(ack.message);
            break;
        }
        connection_close(&connection);
        // Name is probably taken. Add a number.
        attempt++;
        char base[16];
        strncpy(base, options.name, 15);
        base[15] = '\0';
        char *space = strrchr(base, ' ');
        if (space && attempt > 1)
        {
            *space = '\0';
        }
        snprintf(options.name, sizeof(options.name), "%s %d", base, (attempt + 1) % 100);
        usleep(200 * 1000);
    }
    return -1;
}

struct MapObject *find_my_ship(uint8_t object_type)
{
    size_t i = 0;
    while (i < live_state.object_count)
    {
        struct MapObject *object = &live_state.map_objects[i++];
        if (object->owner_id == player_id && object->object_type == object_type)
        {
            return object;
        }
    }
    return NULL;
}

void update_team_from_state()
{
    size_t i = 0;
    while (i < live_state.player_count)
    {
        if (live_state.players[i].id == player_id)
        {
            team_id = live_state.players[i].team_id;
        }
        i++;
    }
}

void place_ship()
{
    struct MapObject ship;
    memset(&ship, 0, sizeof(ship));
    ship.object_type = ship_to_place;
    ship.team_id = team_id;
    ship.owner_id = player_id;
    // Only ships that bot sees are known. Server will say if place is taken.
    if (random_placement(&ship, live_state.map_objects, live_state.object_count, live_state.map_width, live_state.map_height, random_number) != RULE_OK)
    {
        return;
    }
    struct IPlacePacket i_place;
    i_place.object = ship;
    size_t size;
    char *content = i_place_packet_serialization(&i_place, &size);
        send_packet(PACKET_I_PLACE, content, size);
    ship_to_place = 0;
}

/// @brief Is square in placement corner of any enemy team.
int in_enemy_zone_square(int x, int y)
{
    uint8_t team = 1;
    while (team <= MAX_TEAMS)
    {
        if (team != team_id && in_team_zone(team, live_state.map_width, live_state.map_height, x, y))
        {
            return 1;
        }
        team++;
    }
    return 0;
}

/// @brief Picks random square next to earlier hit of our team.
/// @return 1 if found.
int choose_hunt_square(int *x, int *y)
{
    int hits = 0;
    size_t i = 0;
    while (i < live_state.object_count)
    {
        if (live_state.map_objects[i++].object_type == OBJECT_HIT_SQUARE)
        {
            hits++;
        }
    }
    if (!hits)
    {
        return 0;
    }
    int pick = rand() % hits;
    i = 0;
    while (i < live_state.object_count)
    {
        struct MapObject *object = &live_state.map_objects[i++];
        if (object->object_type == OBJECT_HIT_SQUARE && pick-- == 0)
        {
            int dx, dy;
            direction_delta((uint8_t)(rand() % 4), &dx, &dy);
            *x = object->x + dx;
            *y = object->y + dy;
            return *x >= 0 && *y >= 0 && *x < live_state.map_width && *y < live_state.map_height;
        }
    }
    return 0;
}

/// @brief Own ship that can shoot at square.
/// @return Ship type or 0 if no ship reaches it.
uint8_t shooter_for(int x, int y)
{
    uint8_t type = 1;
    while (type <= SHIP_TYPE_COUNT)
    {
        struct MapObject *ship = find_my_ship(type);
        if (ship && validate_shot(ship, x, y, live_state.map_width, live_state.map_height) == RULE_OK)
        {
            return type;
        }
        type++;
    }
    return 0;
}

/// @brief Is square worth a shot: not seen as empty, not own ship, not shot by this bot in current sweep.
int worth_shooting(int x, int y)
{
    if (x < 0 || y < 0 || x >= live_state.map_width || y >= live_state.map_height || my_shots[y * 256 + x])
    {
        return 0;
    }
    // Visible enemies are handled first, so a visible square without them is empty sea.
    return !team_sees_square(team_id, x, y, live_state.map_objects, live_state.object_count) &&
           !team_ship_at(live_state.map_objects, live_state.object_count, team_id, x, y);
}

int aim(struct IGoPacket *action, int x, int y)
{
    uint8_t type = shooter_for(x, y);
    if (!type)
    {
        return 0;
    }
    action->object_type = type;
    action->x = (uint8_t)x;
    action->y = (uint8_t)y;
    return 1;
}

/// @brief Finds ship and target for shot. Visible enemy ships first, then squares next to hits,
/// then sweeps hidden sea (enemy corners first, but ships move so every square gets its turn).
/// @return 1 if found.
int choose_shot(struct IGoPacket *action)
{
    // Visible enemy segments that are not damaged yet.
    size_t i = 0;
    while (i < live_state.object_count)
    {
        struct MapObject *enemy = &live_state.map_objects[i++];
        if (!is_ship(enemy->object_type) || enemy->team_id == team_id || ship_is_sunk(enemy))
        {
            continue;
        }
        int xs[MAX_SHIP_LENGTH], ys[MAX_SHIP_LENGTH];
        int length = ship_squares(enemy, xs, ys);
        int segment = 0;
        while (segment < length)
        {
            if (!(enemy->info & (1 << segment)) && aim(action, xs[segment], ys[segment]))
            {
                return 1;
            }
            segment++;
        }
    }
    // Squares next to earlier hits (ship is probably there).
    int attempt = 0;
    while (attempt++ < 50)
    {
        int x, y;
        if (!choose_hunt_square(&x, &y))
        {
            break;
        }
        if (worth_shooting(x, y) && aim(action, x, y))
        {
            return 1;
        }
    }
    int sweep = 0;
    while (sweep++ < 2)
    {
        // Count squares that can still be tried.
        int in_enemy_zone = 0, elsewhere = 0, y = 0;
        while (y < live_state.map_height)
        {
            int x = 0;
            while (x < live_state.map_width)
            {
                if (worth_shooting(x, y) && shooter_for(x, y))
                {
                    if (in_team_zone(team_id, live_state.map_width, live_state.map_height, x, y) || !in_enemy_zone_square(x, y))
                    {
                        elsewhere++;
                    }
                    else
                    {
                        in_enemy_zone++;
                    }
                }
                x++;
            }
            y++;
        }
        if (in_enemy_zone + elsewhere == 0)
        {
            // Every reachable square was tried. Ships move, so start a new sweep.
            memset(my_shots, 0, sizeof(my_shots));
            continue;
        }
        // Enemy corners first, but not always.
        int use_zone = in_enemy_zone > 0 && (elsewhere == 0 || rand() % 3 != 0);
        int pick = rand() % (use_zone ? in_enemy_zone : elsewhere);
        y = 0;
        while (y < live_state.map_height)
        {
            int x = 0;
            while (x < live_state.map_width)
            {
                if (worth_shooting(x, y) && shooter_for(x, y))
                {
                    int zone = !in_team_zone(team_id, live_state.map_width, live_state.map_height, x, y) && in_enemy_zone_square(x, y);
                    if (zone == use_zone && pick-- == 0)
                    {
                        return aim(action, x, y);
                    }
                }
                x++;
            }
            y++;
        }
    }
    return 0;
}

/// @brief Where ships should go: last hit if there is one, otherwise middle of map where teams meet.
void get_move_target(int *target_x, int *target_y)
{
    *target_x = live_state.map_width / 2;
    *target_y = live_state.map_height / 2;
    size_t i = 0;
    while (i < live_state.object_count)
    {
        struct MapObject *object = &live_state.map_objects[i++];
        if (object->object_type == OBJECT_HIT_SQUARE)
        {
            *target_x = object->x;
            *target_y = object->y;
        }
    }
}

/// @brief Finds ship that can move and direction for it. Mostly moves towards enemies.
/// @return 1 if found.
int choose_move(struct IGoPacket *action)
{
    int target_x, target_y;
    get_move_target(&target_x, &target_y);
    int best_distance = -1, random_pick = rand() % 4 == 0;
    uint8_t type = 1;
    while (type <= SHIP_TYPE_COUNT)
    {
        struct MapObject *ship = find_my_ship(type);
        if (ship && current_turn.moves_left[type - 1] > 0)
        {
            uint8_t direction = 0;
            while (direction < 4)
            {
                struct MapObject moved = *ship;
                if (move_ship(&moved, direction, live_state.map_objects, live_state.object_count, live_state.map_width, live_state.map_height) == RULE_OK)
                {
                    int xs[MAX_SHIP_LENGTH], ys[MAX_SHIP_LENGTH];
                    int head = ship_squares(&moved, xs, ys) - 1;
                    int dx = xs[head] - target_x, dy = ys[head] - target_y;
                    int distance = random_pick ? rand() % 1000 : dx * dx + dy * dy;
                    if (best_distance < 0 || distance < best_distance)
                    {
                        best_distance = distance;
                        action->object_type = type;
                        action->direction = direction;
                    }
                }
                direction++;
            }
        }
        type++;
    }
    return best_distance >= 0;
}

void take_action()
{
    struct IGoPacket action;
    memset(&action, 0, sizeof(action));
    action.player_id = player_id;
    actions_this_turn++;
    if (actions_this_turn > MAX_ACTIONS_PER_TURN)
    {
        action.action = ACTION_END_TURN;
    }
    else if (current_turn.shots_left > 0 && choose_shot(&action))
    {
        action.action = ACTION_SHOOT;
        my_shots[action.y * 256 + action.x] = 1;
    }
    else if (choose_move(&action))
    {
        action.action = ACTION_MOVE;
    }
    else
    {
        action.action = ACTION_END_TURN;
    }
    size_t size;
    char *content = i_go_packet_serialization(&action, &size);
    send_packet(PACKET_I_GO, content, size);
    waiting_answer = 1;
    last_action_ms = now_ms();
}

void process_packet(struct GenericPacket *packet)
{
    switch (packet->packet_type)
    {
    case PACKET_MESSAGE:
    {
        struct MessagePacket message;
        if (message_packet_deserialization(packet->content, packet->packet_content_size, &message) == 0 &&
            (message.message_type == MESSAGE_TYPE_SYSTEM || message.message_type == MESSAGE_TYPE_PLAYER))
        {
            say("[%s] Message: %s", message.message);
        }
        free(message.message);
        break;
    }
    case PACKET_STATE:
        if (state_packet_deserialization(packet->content, packet->packet_content_size, &live_state) == 0)
        {
            update_team_from_state();
        }
        break;
    case PACKET_YOU_PLACE:
    {
        struct YouPlacePacket you_place;
        if (you_place_packet_deserialization(packet->content, packet->packet_content_size, &you_place) == 0 && you_place.player_id == player_id)
        {
            ship_to_place = you_place.object_type;
            if (ship_to_place == 1)
            {
                // New game.
                memset(my_shots, 0, sizeof(my_shots));
            }
            place_at_ms = now_ms() + options.think_ms;
        }
        break;
    }
    case PACKET_YOU_GO:
        if (you_go_packet_deserialization(packet->content, packet->packet_content_size, &current_turn) == 0)
        {
            my_turn = current_turn.player_id == player_id;
            if (current_turn.turn_number != last_turn_number)
            {
                last_turn_number = current_turn.turn_number;
                actions_this_turn = 0;
            }
            waiting_answer = 0;
            next_action_ms = now_ms() + options.think_ms;
        }
        break;
    case PACKET_END_GAME:
    {
        struct EndGamePacket end_game;
        if (end_game_packet_deserialization(packet->content, packet->packet_content_size, &end_game) == 0)
        {
            games_played++;
            my_turn = 0;
            if (end_game.winner_team_id && end_game.winner_team_id == team_id)
            {
                wins++;
            }
            if (!options.quiet)
            {
                log_message(LOG_LEVEL_INFO, "[%s] Game over. Winner team: %d (%s)", options.name, end_game.winner_team_id,
                            end_game.winner_team_id == team_id ? "we won" : "we did not win");
            }
        }
        break;
    }
    default:
        break;
    }
}

void print_help(const char *program)
{
    printf("Usage: %s [options]\n\n", program);
    printf("  --host HOST      Server host (default 127.0.0.1)\n");
    printf("  --port N         Server port (default 12345)\n");
    printf("  --name NAME      Bot name (default Bot)\n");
    printf("  --games N        Games to play before exit, 0 - forever (default 1)\n");
    printf("  --think-ms N     Delay before each action in milliseconds (default 400)\n");
    printf("  --chaos          Testing: send data in tiny pieces and reconnect once during game\n");
    printf("  --quiet          Print only errors\n");
}

int parse_args(int argc, char **argv)
{
    int i = 1;
    while (i < argc)
    {
        const char *argument = argv[i];
        const char *value = i + 1 < argc ? argv[i + 1] : NULL;
        if (strcmp(argument, "--help") == 0)
        {
            print_help(argv[0]);
            exit(0);
        }
        else if (strcmp(argument, "--chaos") == 0)
        {
            options.chaos = 1;
        }
        else if (strcmp(argument, "--quiet") == 0)
        {
            options.quiet = 1;
        }
        else if (value && strcmp(argument, "--host") == 0)
        {
            options.host = value;
            i++;
        }
        else if (value && strcmp(argument, "--port") == 0)
        {
            options.port = atoi(value);
            i++;
        }
        else if (value && strcmp(argument, "--name") == 0)
        {
            snprintf(options.name, sizeof(options.name), "%s", value);
            i++;
        }
        else if (value && strcmp(argument, "--games") == 0)
        {
            options.games = atoi(value);
            i++;
        }
        else if (value && strcmp(argument, "--think-ms") == 0)
        {
            options.think_ms = atoi(value);
            i++;
        }
        else
        {
            fprintf(stderr, "Unknown or incomplete argument: %s (see --help)\n", argument);
            return -1;
        }
        i++;
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (parse_args(argc, argv))
    {
        return 1;
    }
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, handle_stop_signal);
    signal(SIGTERM, handle_stop_signal);
    srand((unsigned int)(now_ms() ^ getpid()));
    memset(&live_state, 0, sizeof(live_state));
    if (connect_and_register())
    {
        log_message(LOG_LEVEL_ERROR, "[%s] Could not connect to %s:%d", options.name, options.host, options.port);
        return 2;
    }
    int exit_code = 0;
    while (running && (options.games == 0 || games_played < options.games))
    {
        connection_flush(&connection);
        struct pollfd poll_info = {.fd = connection.socket, .events = POLLIN};
        poll(&poll_info, 1, options.chaos ? 1 : 20);
        int read_status = connection_read(&connection);
        struct GenericPacket *packet;
        while ((packet = connection_next_packet(&connection)) != NULL)
        {
            process_packet(packet);
            free_generic_packet(packet);
        }
        if (read_status != CONNECTION_OK)
        {
            log_message(LOG_LEVEL_ERROR, "[%s] Lost connection to server", options.name);
            exit_code = 3;
            break;
        }
        int64_t now = now_ms();
        if (options.chaos && !chaos_disconnected && live_state.status == GAME_STATUS_MAIN_GAME && rand() % 400 == 0)
        {
            // Testing: drop connection in the middle of game and come back with same name.
            chaos_disconnected = 1;
            say("[%s] Chaos: %s", "disconnecting on purpose");
            connection_close(&connection);
            usleep((300 + rand() % 700) * 1000);
            my_turn = 0;
            if (connect_and_register())
            {
                exit_code = 4;
                break;
            }
            continue;
        }
        if (ship_to_place && now >= place_at_ms && live_state.status == GAME_STATUS_SHIP_PLACEMENT && live_state.map_width)
        {
            place_ship();
        }
        if (my_turn && live_state.status == GAME_STATUS_MAIN_GAME)
        {
            if ((!waiting_answer && now >= next_action_ms) || (waiting_answer && now - last_action_ms > ACTION_RETRY_MS))
            {
                take_action();
            }
        }
    }
    // Give last packets a chance to leave.
    connection_flush(&connection);
    connection_close(&connection);
    free_state_packet(&live_state);
    if (!options.quiet || exit_code)
    {
        log_message(exit_code ? LOG_LEVEL_ERROR : LOG_LEVEL_INFO, "[%s] Finished. Games: %d Wins: %d", options.name, games_played, wins);
    }
    return exit_code;
}
