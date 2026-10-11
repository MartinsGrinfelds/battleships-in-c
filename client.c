#include <stdio.h>
#include <stdlib.h> // exit(), size_t, malloc
#include "packets/connection.h"
#include "graphical/text_formatter.h" // print_failure()
#include "graphical/ui_functions.h" // get_user_input()
#include "graphical/map.h"
#include "game/game_logic.h"
#include "utils/timer.h"
#include <string.h> // strlen()
#include <signal.h> // signal()
#include "external_libraries/raylib-5.0_linux_amd64/include/raylib.h"
#include <unistd.h> // sleep()

#define DEFAULT_HOST "127.0.0.1"
#define DEFAULT_PORT 12345
#define MAX_CHAT_MESSAGES 200
#define MAX_CHAT_INPUT 120
#define RECONNECT_INTERVAL_MS 2000
#define FEEDBACK_SHOW_MS 6000

#ifdef BATTLESHIPS_VERSION
char* APP_VERSION = BATTLESHIPS_VERSION;
#else
char* APP_VERSION = "UNDEFINED!";
#endif

/// @brief Command line options.
struct ClientOptions
{
    char host[256];
    int port;
    char name[MAX_USERNAME + 1]; // If given, name screen is skipped.
    int width;
    int height;
    char screenshot[256]; // Testing: take screenshot and exit.
    int screenshot_after_seconds;
};

struct ClientOptions options = {DEFAULT_HOST, DEFAULT_PORT, "", 1280, 960, "", 10};

/// @brief Connection to server.
struct Connection connection;
bool connection_active = false;
bool waiting_registration = false;
int64_t next_reconnect_ms = 0;

/// @brief Live state received from the server. Server already hides what fog of war hides.
/// 1. Draw FOW sea.
/// 2. Draw Friendly ships and viewing ranges.
/// 3. Draw other objects.
struct StatePacket live_state;
uint8_t *map = NULL; // Map built from last state.
uint8_t *display_map = NULL; // Map with things that only this client knows (ship being placed).
size_t map_size = 0;

/// @brief Chat messages (linked list, oldest first).
struct Message *messages = NULL;
struct Message *last_message = NULL;
int message_count = 0;
bool chat_active = false;
char chat_input[MAX_CHAT_INPUT + 1] = "";
uint8_t chat_receiver_id = 0; // 0 - everyone.

/// @brief State options.
uint8_t team_id = 0;
uint8_t player_id = 0;
char *user_name = NULL;
char server_message[256] = ""; // Status message from server.
char timer_message[128] = "";
char feedback_message[256] = ""; // Last message from server to this player (for example why action failed).
int64_t feedback_until_ms = 0;
uint8_t ship_to_place = 0; // 0 means no ship to place
int placed_ship_x = -1, placed_ship_y = -1, placed_ship_rotation = ROTATION_RIGHT; // -1 means no ship placed yet
bool placement_locked = false; // Player clicked on square. Ship stays there until Enter.
struct YouGoPacket current_turn;
uint8_t selected_ship = OBJECT_BASE_SHIP;
bool game_over = false;
struct EndGamePacket game_result;
bool needs_new_name = false;
char registration_message[256] = "";

// Messages
#define ASK_FOR_USERNAME "Please Enter Username:"

/// @brief Finds player name in state.
const char *get_player_name(uint8_t id)
{
    if (id == 0)
    {
        return "Server";
    }
    size_t i = 0;
    while (i < live_state.player_count)
    {
        if (live_state.players[i].id == id)
        {
            return live_state.players[i].username;
        }
        i++;
    }
    return "?";
}

/// @brief Adds message to chat. Old messages are forgotten.
void add_chat_message(const char *sender, const char *receiver, const char *text)
{
    struct Message *message = malloc(sizeof(struct Message));
    message->sender_name = strdup(sender);
    message->receiver_name = strdup(receiver);
    message->message = strdup(text);
    message->next_message = NULL;
    if (last_message)
    {
        last_message->next_message = message;
    }
    else
    {
        messages = message;
    }
    last_message = message;
    message_count++;
    if (message_count > MAX_CHAT_MESSAGES)
    {
        struct Message *oldest = messages;
        messages = oldest->next_message;
        free(oldest->sender_name);
        free(oldest->receiver_name);
        free(oldest->message);
        free(oldest);
        message_count--;
    }
}

void send_packet(uint8_t packet_type, char *content, size_t size)
{
    if (connection_active)
    {
        connection_send(&connection, packet_type, content, size);
    }
    free(content);
}

/// @brief Does client startup actions such as window creation.
void startup_client()
{
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(options.width, options.height, "Battleships");
    // Make sure window fits on screen.
    int monitor = GetCurrentMonitor();
    int monitor_width = GetMonitorWidth(monitor), monitor_height = GetMonitorHeight(monitor);
    if (monitor_width > 0 && monitor_height > 0 && (options.width > monitor_width || options.height > monitor_height - 60))
    {
        int width = options.width > monitor_width ? monitor_width * 9 / 10 : options.width;
        int height = options.height > monitor_height - 60 ? (monitor_height - 60) * 9 / 10 : options.height;
        SetWindowSize(width, height);
        SetWindowPosition((monitor_width - width) / 2, (monitor_height - height) / 2);
    }
    SetWindowMinSize(900, 650);
    SetExitKey(KEY_NULL); // Escape is used to close chat, not the game.
    load_ui_fonts();
    connection_init(&connection, -1);
    memset(&live_state, 0, sizeof(live_state));
    signal(SIGPIPE, SIG_IGN);
}

void send_hello()
{
    struct HelloPacket client_hello;
    client_hello.name = user_name;
    client_hello.name_length = (uint8_t)strlen(user_name);
    size_t size;
    char *content = hello_packet_serialization(&client_hello, &size);
    send_packet(PACKET_HELLO, content, size);
}

/// @brief Connects to server and sends HELLO. Answer (ACK) is processed with other packets.
/// @return true if connected.
bool connect_to_server(int timeout_ms)
{
    int socket = connect_to_host(options.host, (uint16_t)options.port, timeout_ms);
    if (socket < 0)
    {
        return false;
    }
    connection_init(&connection, socket);
    connection_active = true;
    waiting_registration = true;
    send_hello();
    return true;
}

void disconnect_from_server()
{
    connection_close(&connection);
    connection_active = false;
    waiting_registration = false;
}

void process_ack(struct GenericPacket *server_packet)
{
    struct AckPacket server_answer;
    if (ack_packet_deserialization(server_packet->content, server_packet->packet_content_size, &server_answer) == 0)
    {
        if (server_answer.player_id)
        {
            player_id = server_answer.player_id;
            team_id = server_answer.team_id;
            waiting_registration = false;
            print_success("Registration Successful!\n");
            printf("Your Player ID: %d\n", player_id);
            printf("Your Team ID: %d\n", team_id);
            add_chat_message("Server", user_name, server_answer.message);
        }
        else
        {
            // Name taken or server full. Player has to choose again.
            snprintf(registration_message, sizeof(registration_message), "%s", server_answer.message);
            needs_new_name = true;
            disconnect_from_server();
        }
    }
    free(server_answer.message);
}

/// @brief Register client
/// @return Returns 1 if succesfull. 0 if no succesful (crazy right?)
int register_client()
{
    int64_t deadline = now_ms() + 8000;
    if (!connect_to_server(3000))
    {
        snprintf(registration_message, sizeof(registration_message), "Can't Connect To %.200s:%d. Is Server Running?", options.host, options.port);
        return 0;
    }
    while (waiting_registration && !WindowShouldClose() && now_ms() < deadline)
    {
        connection_flush(&connection);
        connection_read(&connection);
        struct GenericPacket *server_packet;
        while (waiting_registration && (server_packet = connection_next_packet(&connection)) != NULL)
        {
            if (server_packet->packet_type == PACKET_ACK)
            {
                process_ack(server_packet);
            }
            free_generic_packet(server_packet);
        }
        if (connection.state != CONNECTION_OK && waiting_registration)
        {
            break;
        }
        BeginDrawing();
            ClearBackground(FOW_SEA_COLOR);
            draw_center_message("Connecting...", TextFormat("%s:%d", options.host, options.port));
        EndDrawing();
    }
    if (!connection_active || waiting_registration)
    {
        if (!needs_new_name)
        {
            snprintf(registration_message, sizeof(registration_message), "Server Did Not Answer. Try Again.");
        }
        disconnect_from_server();
        return 0;
    }
    return 1;
}

/// @brief Builds ship that player is placing now.
struct MapObject get_placed_ship()
{
    struct MapObject ship;
    memset(&ship, 0, sizeof(ship));
    ship.object_type = ship_to_place;
    ship.x = (uint8_t)placed_ship_x;
    ship.y = (uint8_t)placed_ship_y;
    ship.rotation = (uint8_t)placed_ship_rotation;
    ship.team_id = team_id;
    ship.owner_id = player_id;
    return ship;
}

/// @brief Ship placement (INTR-0008, INTR-0009, INTR-0010).
void place_ship(uint8_t ship_type)
{
    if (ship_type == 0 || live_state.status != GAME_STATUS_SHIP_PLACEMENT) {
        // No ship to place.
        return;
    }
    // 1. Get player pressed coordinates. Before click ship follows mouse.
    int x, y;
    Vector2 mouse_pos = GetMousePosition();
    bool on_map = get_map_square_from_absolute(live_state.map_width, live_state.map_height, mouse_pos.x, mouse_pos.y, &x, &y) == 0;
    if (on_map && (!placement_locked || IsMouseButtonPressed(MOUSE_BUTTON_LEFT)))
    {
        // 2. Convert coordinates to map square X,Y.
        placed_ship_x = x;
        placed_ship_y = y;
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            placement_locked = true;
        }
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
    {
        placement_locked = false;
    }
    // 3. Allow rotation.
    if (IsKeyPressed(KEY_R))
    {
        placed_ship_rotation = (placed_ship_rotation + 1) % 4;
    }
    // 4. Once player confirms placement send data to server. Server checks it again.
    if ((IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) && placed_ship_x >= 0)
    {
        struct MapObject ship = get_placed_ship();
        int result = validate_placement(&ship, live_state.map_objects, live_state.object_count, live_state.map_width, live_state.map_height);
        if (result != RULE_OK)
        {
            snprintf(feedback_message, sizeof(feedback_message), "%s", rule_text(result));
            feedback_until_ms = now_ms() + FEEDBACK_SHOW_MS;
            return;
        }
        struct IPlacePacket i_place_packet;
        i_place_packet.object = ship;
        size_t size;
        char *content = i_place_packet_serialization(&i_place_packet, &size);
        send_packet(PACKET_I_PLACE, content, size);
        // Reset placement data. Server sends next ship type (YouPlace).
        ship_to_place = 0;
        placement_locked = false;
        print_success("Ship placement sent to server!\n");
    }
}

/// @brief Is it this player's turn.
bool is_my_turn()
{
    return live_state.status == GAME_STATUS_MAIN_GAME && current_turn.player_id == player_id && player_id != 0;
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

void send_action(uint8_t action, uint8_t x, uint8_t y, uint8_t direction)
{
    struct IGoPacket i_go;
    i_go.player_id = player_id;
    i_go.action = action;
    i_go.x = x;
    i_go.y = y;
    i_go.object_type = selected_ship;
    i_go.direction = direction;
    size_t size;
    char *content = i_go_packet_serialization(&i_go, &size);
    send_packet(PACKET_I_GO, content, size);
}

/// @brief Main game controls (INTR-0014 - INTR-0017).
void handle_game_input()
{
    // Ship selection with number keys (top row or numpad).
    int key = KEY_ONE;
    while (key <= KEY_FIVE)
    {
        if (IsKeyPressed(key) || IsKeyPressed(KEY_KP_1 + (key - KEY_ONE)))
        {
            selected_ship = (uint8_t)(key - KEY_ONE + 1);
        }
        key++;
    }
    if (!is_my_turn())
    {
        return;
    }
    int directions[4][2] = {{KEY_UP, ROTATION_UP}, {KEY_RIGHT, ROTATION_RIGHT}, {KEY_DOWN, ROTATION_DOWN}, {KEY_LEFT, ROTATION_LEFT}};
    int i = 0;
    while (i < 4)
    {
        if (IsKeyPressed(directions[i][0]))
        {
            send_action(ACTION_MOVE, 0, 0, (uint8_t)directions[i][1]);
        }
        i++;
    }
    int x, y;
    Vector2 mouse_pos = GetMousePosition();
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
        get_map_square_from_absolute(live_state.map_width, live_state.map_height, mouse_pos.x, mouse_pos.y, &x, &y) == 0)
    {
        send_action(ACTION_SHOOT, (uint8_t)x, (uint8_t)y, 0);
    }
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
    {
        send_action(ACTION_END_TURN, 0, 0, 0);
    }
}

/// @brief Next player who can receive private message (0 - everyone).
uint8_t next_chat_receiver(uint8_t current)
{
    // Players are in state list. Find current and take next one (not self).
    int index = -1;
    size_t i = 0;
    while (i < live_state.player_count)
    {
        if (live_state.players[i].id == current)
        {
            index = (int)i;
        }
        i++;
    }
    i = (size_t)(index + 1);
    while (i < live_state.player_count)
    {
        struct Player *player = &live_state.players[i];
        if (player->id != player_id && player->status != PLAYER_STATUS_DISCONNECTED)
        {
            return player->id;
        }
        i++;
    }
    return 0;
}

/// @brief Chat (INTR-0002 - INTR-0005). T opens chat, Tab chooses receiver, Enter sends, Escape closes.
/// @return true if chat used keyboard this frame.
bool handle_chat_input()
{
    if (!chat_active)
    {
        if (IsKeyPressed(KEY_T))
        {
            chat_active = true;
            // Letter T is also in character queue. Clean it.
            while (GetCharPressed() != 0)
            {
            }
            return true;
        }
        return false;
    }
    int key = GetCharPressed();
    while (key > 0)
    {
        size_t length = strlen(chat_input);
        if (key >= 32 && key <= 125 && length < MAX_CHAT_INPUT)
        {
            chat_input[length] = (char)key;
            chat_input[length + 1] = '\0';
        }
        key = GetCharPressed();
    }
    if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && chat_input[0])
    {
        chat_input[strlen(chat_input) - 1] = '\0';
    }
    if (IsKeyPressed(KEY_TAB))
    {
        chat_receiver_id = next_chat_receiver(chat_receiver_id);
    }
    if (IsKeyPressed(KEY_ESCAPE))
    {
        chat_active = false;
    }
    if ((IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)))
    {
        if (chat_input[0])
        {
            struct MessagePacket message;
            message.message_type = MESSAGE_TYPE_PLAYER;
            message.sender_id = player_id;
            message.receiver_id = chat_receiver_id;
            message.message = chat_input;
            message.message_length = (uint8_t)strlen(chat_input);
            size_t size;
            char *content = message_packet_serialization(&message, &size);
            send_packet(PACKET_MESSAGE, content, size);
            chat_input[0] = '\0';
        }
        chat_active = false;
    }
    return true;
}

int process_message_packet(struct MessagePacket *message_packet)
{
    if (!message_packet->message)
    {
        print_failure("Empty server message provided to process!\n");
        return 1;
    }
    switch (message_packet->message_type)
    {
    case MESSAGE_TYPE_SYSTEM:
        add_chat_message("Server", message_packet->receiver_id ? get_player_name(message_packet->receiver_id) : "All", message_packet->message);
        if (message_packet->receiver_id == player_id)
        {
            // Message only for this player (for example why action failed). Show it in status area too.
            snprintf(feedback_message, sizeof(feedback_message), "%s", message_packet->message);
            feedback_until_ms = now_ms() + FEEDBACK_SHOW_MS;
        }
        break;
    case MESSAGE_TYPE_PLAYER:
        add_chat_message(get_player_name(message_packet->sender_id), message_packet->receiver_id ? get_player_name(message_packet->receiver_id) : "All",
                         message_packet->message);
        break;
    case MESSAGE_TYPE_TIMER:
        snprintf(timer_message, sizeof(timer_message), "%s", message_packet->message);
        break;
    case MESSAGE_TYPE_STATUS:
        snprintf(server_message, sizeof(server_message), "%s", message_packet->message);
        break;
    default:
        print_failure("BUG (report to developers)! ");
        printf("Unknown server message type provided to process: %d\n", message_packet->message_type);
        return 2;
    }
    return 0;
}

/// @brief Makes sure map memory fits map size from server.
void prepare_map()
{
    size_t needed = (size_t)live_state.map_width * live_state.map_height;
    if (needed != map_size)
    {
        free(map);
        free(display_map);
        map = calloc(needed ? needed : 1, sizeof(uint8_t));
        display_map = calloc(needed ? needed : 1, sizeof(uint8_t));
        map_size = needed;
    }
}

void process_state()
{
    // Team can change when lobby closes (teams are balanced).
    size_t i = 0;
    while (i < live_state.player_count)
    {
        if (live_state.players[i].id == player_id)
        {
            team_id = live_state.players[i].status == PLAYER_STATUS_OBSERVER ? 0 : live_state.players[i].team_id;
        }
        i++;
    }
    if (live_state.status == GAME_STATUS_LOBBY)
    {
        game_over = false;
        ship_to_place = 0;
        memset(&current_turn, 0, sizeof(current_turn));
    }
    prepare_map();
    update_map_with_objects(&live_state, map, team_id, player_id);
}

/// @brief Try to receive packets from server and process them accordingly.
int process_server_packet()
{
    // In case of not active connection do nothing.
    if (!connection_active)
    {
        return 1;
    }
    connection_flush(&connection);
    connection_read(&connection);
    struct GenericPacket *server_packet;
    while (connection_active && (server_packet = connection_next_packet(&connection)) != NULL)
    {
        switch (server_packet->packet_type)
        {
        case PACKET_ACK:
            process_ack(server_packet);
            break;
        case PACKET_MESSAGE:
        {
            // MESSAGE packet process
            struct MessagePacket received_message;
            if (message_packet_deserialization(server_packet->content, server_packet->packet_content_size, &received_message) == 0)
            {
                process_message_packet(&received_message);
            }
            free(received_message.message);
            break;
        }
        case PACKET_STATE:
            // STATE packet received
            if (state_packet_deserialization(server_packet->content, server_packet->packet_content_size, &live_state) == 0)
            {
                process_state();
            }
            break;
        case PACKET_YOU_PLACE:
        {
            // YouPlace packet received
            struct YouPlacePacket you_place_packet;
            if (you_place_packet_deserialization(server_packet->content, server_packet->packet_content_size, &you_place_packet) == 0 &&
                you_place_packet.player_id == player_id)
            {
                // Now call function for ship placement. Not directly because we need to update UI all the time.
                ship_to_place = you_place_packet.object_type;
                placement_locked = false;
            }
            break;
        }
        case PACKET_YOU_GO:
        {
            struct YouGoPacket you_go;
            if (you_go_packet_deserialization(server_packet->content, server_packet->packet_content_size, &you_go) == 0)
            {
                bool new_turn = you_go.turn_number != current_turn.turn_number;
                current_turn = you_go;
                struct MapObject *ship = find_my_ship(selected_ship);
                if (new_turn && current_turn.player_id == player_id && (!ship || ship_is_sunk(ship)))
                {
                    // Select first ship that is still afloat.
                    uint8_t type = 1;
                    while (type <= SHIP_TYPE_COUNT)
                    {
                        ship = find_my_ship(type);
                        if (ship && !ship_is_sunk(ship))
                        {
                            selected_ship = type;
                            break;
                        }
                        type++;
                    }
                }
            }
            break;
        }
        case PACKET_END_GAME:
            if (end_game_packet_deserialization(server_packet->content, server_packet->packet_content_size, &game_result) == 0)
            {
                game_over = true;
            }
            break;
        default:
            print_warning("Unknown packet received from server: ");
            printf("Type (%d)\n", server_packet->packet_type);
        }
        free_generic_packet(server_packet);
    }
    if (connection_active && connection.state != CONNECTION_OK)
    {
        // Lost connection. Try to come back (server keeps our ships for a while).
        disconnect_from_server();
        add_chat_message("Server", "You", "Connection Lost. Reconnecting...");
        next_reconnect_ms = now_ms() + RECONNECT_INTERVAL_MS;
    }
    return 0;
}

/// @brief Tries to reconnect if connection is lost.
void keep_connection()
{
    if (connection_active || needs_new_name || now_ms() < next_reconnect_ms)
    {
        return;
    }
    if (!connect_to_server(300))
    {
        next_reconnect_ms = now_ms() + RECONNECT_INTERVAL_MS;
    }
}

/// @brief Text about own ships for status area.
void build_ships_text(char *text, size_t size)
{
    text[0] = '\0';
    size_t used = 0;
    uint8_t type = 1;
    while (type <= SHIP_TYPE_COUNT)
    {
        struct MapObject *ship = find_my_ship(type);
        const struct ShipProperties *properties = get_ship_properties(type);
        if (ship)
        {
            const char *condition = ship_is_sunk(ship) ? "Sunk" : TextFormat("%d/%d", ship_healthy_segments(ship), properties->length);
            int moves = is_my_turn() ? current_turn.moves_left[type - 1] : (ship_is_damaged(ship) ? 0 : properties->speed);
            used += snprintf(text + used, size - used, "%s%d %s %s Moves %d\n", type == selected_ship ? "> " : "  ", type, properties->name, condition, moves);
        }
        type++;
    }
}

/// @brief Text telling player what to do now.
void build_action_text(char *text, size_t size)
{
    text[0] = '\0';
    if (!connection_active)
    {
        snprintf(text, size, "Connection Lost. Reconnecting...");
        return;
    }
    switch (live_state.status)
    {
    case GAME_STATUS_LOBBY:
        snprintf(text, size, "Waiting For Game To Start.");
        break;
    case GAME_STATUS_SHIP_PLACEMENT:
        if (ship_to_place)
        {
            const struct ShipProperties *properties = get_ship_properties(ship_to_place);
            snprintf(text, size, "Place %s (Length %d).\nClick In Your Zone, R To Rotate, Enter To Confirm.", properties ? properties->name : "?",
                     properties ? properties->length : 0);
        }
        else
        {
            snprintf(text, size, team_id ? "Waiting On Others..." : "Players Are Placing Ships.");
        }
        break;
    case GAME_STATUS_MAIN_GAME:
        if (is_my_turn())
        {
            snprintf(text, size, "YOUR TURN! Shots Left: %d\n1-5 Select Ship, Arrows Move, Click To Shoot, Enter Ends Turn.", current_turn.shots_left);
        }
        else if (current_turn.player_id)
        {
            snprintf(text, size, "Turn: %s", get_player_name(current_turn.player_id));
        }
        else
        {
            snprintf(text, size, "Waiting For Players To Reconnect...");
        }
        break;
    case GAME_STATUS_GAME_OVER:
        snprintf(text, size, "Game Over.");
        break;
    }
    if (feedback_message[0] && now_ms() < feedback_until_ms)
    {
        size_t used = strlen(text);
        snprintf(text + used, size - used, "\n\n%s", feedback_message);
    }
}

/// @brief Draws map with things that only this client knows.
void draw_game_map()
{
    if (!map || !live_state.map_width)
    {
        return;
    }
    memcpy(display_map, map, map_size);
    if (ship_to_place && placed_ship_x >= 0 && live_state.status == GAME_STATUS_SHIP_PLACEMENT)
    {
        struct MapObject ship = get_placed_ship();
        bool valid = validate_placement(&ship, live_state.map_objects, live_state.object_count, live_state.map_width, live_state.map_height) == RULE_OK;
        add_object(display_map, live_state.map_width, live_state.map_height, &ship, valid ? PLACED_SHIP : INVALID_PLACED_SHIP,
                   valid ? PLACED_SHIP_TAIL : INVALID_PLACED_SHIP);
    }
    draw_map_area(live_state.map_width, live_state.map_height, display_map);
    draw_map_overlays(&live_state, player_id, live_state.status == GAME_STATUS_MAIN_GAME ? selected_ship : 0);
    // Square under mouse. Green if selected ship can shoot there.
    int x, y;
    Vector2 mouse_pos = GetMousePosition();
    if (live_state.status == GAME_STATUS_MAIN_GAME &&
        get_map_square_from_absolute(live_state.map_width, live_state.map_height, mouse_pos.x, mouse_pos.y, &x, &y) == 0)
    {
        struct MapObject *ship = find_my_ship(selected_ship);
        bool can_shoot = ship && validate_shot(ship, x, y, live_state.map_width, live_state.map_height) == RULE_OK &&
                         !team_ship_at(live_state.map_objects, live_state.object_count, team_id, x, y);
        draw_square_highlight(live_state.map_width, live_state.map_height, x, y, 2, can_shoot ? 0x7CFC00FF : 0xEB5C6DFF);
    }
    if (is_my_turn())
    {
        struct MapLayout layout = get_map_layout(live_state.map_width, live_state.map_height);
        DrawRectangleLinesEx((Rectangle){(float)layout.x, (float)layout.y, (float)(layout.cell * live_state.map_width), (float)(layout.cell * live_state.map_height)},
                             3, SELECTED_COLOR);
    }
    if (!connection_active)
    {
        draw_end_game_overlay(live_state.map_width, live_state.map_height, "Connection Lost", "Reconnecting...");
    }
    else if (game_over && live_state.status == GAME_STATUS_GAME_OVER)
    {
        const char *title, *subtitle;
        if (game_result.winner_team_id == 0)
        {
            title = "Draw!";
        }
        else if (team_id && game_result.winner_team_id == team_id)
        {
            title = "Victory!";
        }
        else
        {
            title = TextFormat("Team %d Wins!", game_result.winner_team_id);
        }
        subtitle = game_result.winner_id ? TextFormat("Best Player: %s. New Game Starts Soon.", get_player_name(game_result.winner_id)) : "New Game Starts Soon.";
        // TextFormat uses rotating buffers so both strings stay valid here.
        draw_end_game_overlay(live_state.map_width, live_state.map_height, title, subtitle);
    }
}

void clientloop()
{
    SetTargetFPS(60);
    int64_t started_ms = now_ms();
    char action_text[512], ships_text[512], chat_line[256];
    while (!WindowShouldClose() && !needs_new_name)
    {
        // Receive packets from server and process them based on type.
        keep_connection();
        process_server_packet();
        if (!handle_chat_input())
        {
            if (live_state.status == GAME_STATUS_SHIP_PLACEMENT)
            {
                place_ship(ship_to_place);
            }
            else if (live_state.status == GAME_STATUS_MAIN_GAME)
            {
                handle_game_input();
            }
        }
        build_action_text(action_text, sizeof(action_text));
        build_ships_text(ships_text, sizeof(ships_text));
        if (chat_active)
        {
            snprintf(chat_line, sizeof(chat_line), "To %s (Tab Changes) > %s_", chat_receiver_id ? get_player_name(chat_receiver_id) : "All", chat_input);
        }
        else
        {
            snprintf(chat_line, sizeof(chat_line), "Press T To Chat");
        }
        BeginDrawing();
            ClearBackground(BLACK);
            draw_game_map();
            draw_status_area(user_name, player_id, team_id, &live_state, server_message, action_text, timer_message, ships_text);
            show_chat_messages(messages, chat_line, chat_active);
        EndDrawing();
        if (options.screenshot[0] && now_ms() - started_ms > options.screenshot_after_seconds * 1000)
        {
            // Testing: picture of window and exit.
            TakeScreenshot(options.screenshot);
            break;
        }
    }
}

void free_messages() {
    struct Message *current = messages;
    struct Message *next = NULL;  // Otherwise issues with accessing next after freeing previous

    while (current != NULL) {
        next = current->next_message;
        free(current->sender_name);
        free(current->receiver_name);
        free(current->message);
        free(current);
        current = next;
    }
    messages = NULL;
    last_message = NULL;
    message_count = 0;
}

/// @brief Executes client shutdown actions (such as main socket closing).
void shutdown_client()
{
    free_messages();
    free_state_packet(&live_state);
    free(map);
    free(display_map);
    free(user_name);
    disconnect_from_server();
    unload_ui_fonts();
    CloseWindow();
    print_success("Client Shutdown Successful!\n");
}

void print_help(const char *program)
{
    printf("Usage: %s [options]\n\n", program);
    printf("  --host HOST      Server host name or IP (default %s)\n", DEFAULT_HOST);
    printf("  --port N         Server port (default %d)\n", DEFAULT_PORT);
    printf("  --name NAME      Player name (skips name screen)\n");
    printf("  --size WxH       Window size (default 1280x960)\n");
    printf("  --version        Show version\n");
    printf("  --screenshot FILE --screenshot-after N\n");
    printf("                   Testing: save picture of window after N seconds and exit\n");
}

/// @brief Reads command line options.
/// @return 0 to continue, 1 to exit successfully, -1 on error.
int parse_args(int argc, char **argv)
{
    int i = 1;
    while (i < argc)
    {
        const char *argument = argv[i];
        const char *value = i + 1 < argc ? argv[i + 1] : NULL;
        if (strcmp(argument, "--help") == 0 || strcmp(argument, "-h") == 0)
        {
            print_help(argv[0]);
            return 1;
        }
        if (strcmp(argument, "--version") == 0)
        {
            printf("%s\n", APP_VERSION);
            return 1;
        }
        if (!value)
        {
            fprintf(stderr, "Unknown or incomplete argument: %s (see --help)\n", argument);
            return -1;
        }
        if (strcmp(argument, "--host") == 0)
        {
            snprintf(options.host, sizeof(options.host), "%s", value);
        }
        else if (strcmp(argument, "--port") == 0)
        {
            options.port = atoi(value);
        }
        else if (strcmp(argument, "--name") == 0)
        {
            snprintf(options.name, sizeof(options.name), "%s", value);
        }
        else if (strcmp(argument, "--size") == 0)
        {
            if (sscanf(value, "%dx%d", &options.width, &options.height) != 2)
            {
                fprintf(stderr, "Size must look like 1280x960\n");
                return -1;
            }
        }
        else if (strcmp(argument, "--screenshot") == 0)
        {
            snprintf(options.screenshot, sizeof(options.screenshot), "%s", value);
        }
        else if (strcmp(argument, "--screenshot-after") == 0)
        {
            options.screenshot_after_seconds = atoi(value);
        }
        else
        {
            fprintf(stderr, "Unknown argument: %s (see --help)\n", argument);
            return -1;
        }
        i += 2;
    }
    if (options.port <= 0 || options.port > 65535)
    {
        fprintf(stderr, "Port must be from 1 to 65535\n");
        return -1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    printf("Welcome to Battleships Client!\n");
    printf("Version: %s\n", APP_VERSION);
    int parse_result = parse_args(argc, argv);
    if (parse_result != 0)
    {
        return parse_result > 0 ? 0 : 1;
    }

    startup_client();
    snprintf(registration_message, sizeof(registration_message), ASK_FOR_USERNAME);
    char server_text[300];
    snprintf(server_text, sizeof(server_text), "%s:%d", options.host, options.port);
    while (!WindowShouldClose())
    {
        // Name from command line is used only on first try.
        if (options.name[0])
        {
            user_name = strdup(options.name);
            options.name[0] = '\0';
        }
        else
        {
            user_name = get_username_input(MIN_USERNAME, MAX_USERNAME, registration_message, server_text);
        }
        if (!user_name)
        {
            // User probably closed window.
            break;
        }
        needs_new_name = false;
        if (register_client())
        {
            clientloop();
            if (!needs_new_name)
            {
                break;
            }
        }
        free(user_name);
        user_name = NULL;
    }

    shutdown_client();
    return 0;
}
