#include <stdio.h>
#include <stdlib.h> // exit(), size_t, malloc
#include "packets/connection.h"
#include "graphical/text_formatter.h" // print_failure()
#include "graphical/ui_functions.h" // get_user_input()
#include "graphical/map.h"
#include <string.h> // strlen()
#include "external_libraries/raylib-5.0_linux_amd64/include/raylib.h"
// #include "external_libraries/raylib-5.0_linux_amd64/include/raymath.h"
// #include "external_libraries/raylib-5.0_linux_amd64/include/rlgl.h"
#include <unistd.h> // sleep()

#define HOST "127.0.0.1"
#define PORT 12345

#ifdef BATTLESHIPS_VERSION
char* APP_VERSION = BATTLESHIPS_VERSION;
#else
char* APP_VERSION = "UNDEFINED!";
#endif

/// @brief Represents TCP socket that connects to server.
int client_tcp_socket = -1;

/// @brief TODO: Packet Nr check to validate if server packet is correct.
int client_packet_nr = 0;

/// @brief Live state received from the server. Client must calculate FOW effects from this using allied ship coordinates.
/// 1. Draw FOW sea.
/// 2. Draw Friendly ships and viewing ranges.
/// 3. Draw other objects.
struct StatePacket live_state;
uint8_t *map = NULL;

/// @brief TEMPORARY (to be removed)
struct Message messages;

/// @brief Game options.
bool allow_offline = true;

/// @brief State options.
bool connection_active = false;
uint8_t team_id = 0;
uint8_t player_id = 0;

// Messages
#define ASK_FOR_USERNAME "Please Enter Username:\0"

/// @brief Does client startup actions such as socket creation, binding, connection.
void startup_client()
{
    SetTraceLogLevel(LOG_WARNING);

    // TEMPORARY BLOCK START
    live_state.map_width = 50;
    live_state.map_height = 50;
    live_state.object_count = 1;
    live_state.map_objects = calloc(1, sizeof(struct MapObject));
    live_state.map_objects->object_type = 1; // Base ship
    live_state.map_objects->team_id = 1;
    live_state.map_objects->rotation = 0;
    live_state.map_objects->x = 10;
    live_state.map_objects->y = 10;
    InitWindow(1200, 1200, "Battleships");
    messages.message = malloc(sizeof("You look awesome!\0"));
    messages.message = "You look awesome!\0";
    messages.sender_name = malloc(sizeof("Zenitsu\0"));
    messages.sender_name = "Zenitsu\0";
    messages.receiver_name = malloc(sizeof("Nezuko\0"));
    messages.receiver_name = "Nezuko\0";

    messages.next_message = malloc(sizeof(struct Message));
    struct Message* current_message = messages.next_message;
    current_message->message = malloc(sizeof("Aww. Thanks! Tho you don't see me lol\0"));
    current_message->message = "Aww. Thanks! Tho you don't see me lol\0";
    current_message->sender_name = malloc(sizeof("Nezuko\0"));
    current_message->sender_name = "Nezuko\0";
    current_message->receiver_name = malloc(sizeof("Zenitsu\0"));
    current_message->receiver_name = "Zenitsu\0";
    current_message->next_message = NULL;
    // map[22] = 1; // Only test REMOVE
    // map[68] = 2; // Only test REMOVE
    // map[69] = 2; // Only test REMOVE
    // map[225] = 1; // Only test REMOVE
    // TEMPORARY BLOCK END

    // Call socket creation
    client_tcp_socket = create_socket();
    if(connect_socket(client_tcp_socket, PORT, HOST) != 0)
    {
        if (allow_offline)
        {
            return;
        }
        print_failure("Connection to server failed!\n");
        close_socket(client_tcp_socket);
        CloseWindow();
        exit(1);
    }
    connection_active = true;
}

/// @brief Register client
/// @return Returns 1 if succesfull. 0 if no succesful (crazy right?)
int register_client()
{
    if (!connection_active && allow_offline)
    {
        return 1;
    }
    else if (!connection_active)
    {
        return 0;
    }
    // Creating this to imitate server response already at the geginning as it is asking for username.
    struct AckPacket server_answer;
    server_answer.player_id = 0;
    server_answer.message = ASK_FOR_USERNAME;
    while (server_answer.player_id == 0)
    {
        char *user_name = get_username_input(MIN_USERNAME, MAX_USERNAME, server_answer.message);
        if (!user_name)
        {
            // User probably closed window.
            return 0;
        }
        struct HelloPacket client_hello;
        client_hello.name = user_name;
        client_hello.name_length = (uint8_t)strlen(user_name);
        size_t serialized_packet_size;
        char *serialized_packet = hello_packet_serialization(&client_hello, &serialized_packet_size);
        struct GenericPacket generic_hello;
        generic_hello.packet_type = 0;
        generic_hello.packet_content_size = serialized_packet_size;
        generic_hello.content = serialized_packet;
        generic_hello.checksum = 0;
        send_generic_packet(client_tcp_socket, &generic_hello);
        free(user_name);
        free(serialized_packet);
        struct GenericPacket *server_packet = receive_generic_packet(client_tcp_socket);

        if(server_packet->packet_type != 1)
        {
            print_failure("FATAL REGISTRATION FAILURE! REPORT TO DEVS!\n");
            printf("Packet type that caused failure: %d\n", server_packet->packet_type);
            // printf("Error: %d/n", server_packet->packet_type);
            close_socket(client_tcp_socket);
            return 0;
        }
        ack_packet_deserialization(server_packet->content, &server_answer);
        printf("Server message: %s\n", server_answer.message);
        
        free(server_packet->content);
        free(server_packet);
    }
    print_success("Registration Successful!\n");
    player_id = server_answer.player_id;
    team_id = server_answer.team_id;
    printf("Your Player ID: %d\n", player_id);
    printf("Your Team ID: %d\n", team_id);
    return 1;
}

/// @brief Try to receive a packet from server and process it accordingly.
/// @return TODO: define this (should be action to take after packet)
int process_server_packet()
{
    // In case of not active connection do nothing.
    if (!connection_active)
    {
        return 0;
    }
    struct GenericPacket *server_packet = receive_generic_packet(client_tcp_socket);
    if (server_packet == NULL)
    {
        // Server did not send anything.
        return 0;
    }
    // TODO: Verify sequence number.

    // TODO: Verify checksum.

    switch (server_packet->packet_type)
    {
    case 2:
        // TODO: MESSAGE packet process
        print_failure("Received MESSAGE from server but processing is not yet implemented.\n");
        break;
    
    case 3:
        // STATE packet received
        printf("DEBUG: State packet received.");
        // state_packet_deserialization(server_packet->content, &live_state);
        update_map_with_objects(&live_state, map, team_id);
        break;

    default:
        print_warning("BUG (report to developers)!\n");
        print_warning("Unknown packet received from server: ");
        printf("Type (%d)\n", server_packet->packet_type);
    }
    return server_packet->packet_type;
}

void clientloop()
{
    set_socket_non_blocking(client_tcp_socket); // Make sure socket is non-blocking.
    SetTargetFPS(5); // TODO: Fix this setting due to fast packet receiving or remove this comment.
    while (!WindowShouldClose())
    {
        // Receive packet from server and process it based on type.
        process_server_packet();
        BeginDrawing();
            draw_map_area(live_state.map_width, live_state.map_height, map);
            show_chat_messages(&messages);
        EndDrawing();
        // DEMO STARTS
        update_map_with_objects(&live_state, map, team_id); // TODO: REMOVE!!!!!
        if (++live_state.map_objects->rotation > 3)
        {
            live_state.map_objects->rotation = 0;
        }
        if (++live_state.map_objects->x > 35)
        {
            live_state.map_objects->x = 10;
        }
        if (++live_state.map_objects->y > 40)
        {
            live_state.map_objects->y = 10;
        }
        // DEMO ENDS
    }
}

/// @brief Executes client shutdown actions (such as main socket closing).
void shutdown_client()
{
    if (live_state.map_objects)
    {
        free(live_state.map_objects);
    }
    if (live_state.players)
    {
        free(live_state.players);
    }
    if (map)
    {
        free(map);
    }
    close_socket(client_tcp_socket);
    CloseWindow();
    print_success("Client Shutdown Successful!\n");
}

int main()
{
    printf("Welcome to Battleships Client!\n");
    printf("Version: %s\n", APP_VERSION);

    startup_client();
    if (register_client())
    {
        ClearBackground(BLACK);
        // Checking if map squares can be correctly displayed (aka square size is not float)
        // If square size if float then 
        int screen_x = GetScreenWidth(), screen_y = GetScreenHeight();
        screen_x = screen_x * 3 / 4; // Potencial hardcode
        screen_y = screen_y * 3 / 4; // Potencial hardcode
        if (live_state.map_width == 0 || live_state.map_height == 0)
        {
            print_warning("Map size not defined! Setting fake one: ");
            live_state.map_width = 50;
            live_state.map_height = 50;
            printf("W: %d, H:%d\n", live_state.map_width, live_state.map_height);
        }
        else if (screen_x%(int)(screen_x/live_state.map_width) || screen_y%(int)(screen_y/live_state.map_height))
        {
            print_warning("Not Great Resolution and map size combination! Squares will adjust in size.\n");
        }
        map = calloc(live_state.map_width * live_state.map_height, sizeof(uint8_t));
        // TODO: Fix previous lines -> make better inicialization.
        clientloop();
    }

    shutdown_client();
    return 0;

}