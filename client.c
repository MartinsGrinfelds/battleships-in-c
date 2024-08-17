#include <stdio.h>
#include <stdlib.h> // exit(), size_t, malloc
#include "packets/connection.h"
#include "graphical/text_formatter.h" // print_failure()
#include "graphical/ui_functions.h" // get_user_input()
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

int client_tcp_socket = -1;
int client_packet_nr = 0; // Not used yet. To be implemented.
uint8_t map_size_x = 20, map_size_y = 20;
uint8_t* map;
struct Message messages;
bool allow_offline = true, connection_active = false;

/// @brief Does client startup actions such as socket creation, binding, connection.
void startup_client()
{
    InitWindow(1000, 1000, "Battleships");
    map = calloc(map_size_x*map_size_y, sizeof(uint8_t));
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
    map[22] = 1; // Only test REMOVE
    map[68] = 2; // Only test REMOVE
    map[69] = 2; // Only test REMOVE
    map[225] = 1; // Only test REMOVE
    

    // while (!WindowShouldClose())
    // {
    //     // if (IsWindowResized())
    //     // {
    //     draw_map_area(map_size_x, map_size_y, map); // REMOVE THIS
    //     // show_chat_messages();
    //     // }
    // }

    // ToggleFullscreen();
    // while (!WindowShouldClose())
    // {
    //     BeginDrawing();
    //     ClearBackground(GRAY);
    //     DrawText("Welcome to Battleships!", 190, 200, 40, BLACK);
    //     EndDrawing();
    // }
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
    char *user_name = get_username_input(30); // Check if NULL
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
        print_failure("Registration failed!\n");
        close_socket(client_tcp_socket);
        return 0;
    }
    print_success("Registration Successful!\n");
    printf("Your Player ID: %d\n", ((struct AckPacket *)server_packet->content)->player_id);
    printf("Your Team ID: %d\n", ((struct AckPacket *)server_packet->content)->team_id);
    free(server_packet->content);
    free(server_packet);
    return 1;
}

void clientloop()
{
    while (!WindowShouldClose())
    {
        BeginDrawing();
        draw_map_area(map_size_x, map_size_y, map);
        show_chat_messages(&messages);
        EndDrawing();
        sleep(1);
    }
}

/// @brief Executes client shutdown actions (such as main socket closing).
void shutdown_client()
{
    free(map);
    close_socket(client_tcp_socket);
    CloseWindow();
}

int main()
{
    printf("Welcome to Battleships Client!\n");
    printf("Version: %s\n", APP_VERSION);

    startup_client();
    if (register_client())
    {
        clientloop();
    }

    shutdown_client();
    return 0;

}