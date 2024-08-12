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

/// @brief Does client startup actions such as socket creation, binding, connection.
void startup_client()
{
    InitWindow(1000, 1000, "Battleships");
    map = calloc(map_size_x*map_size_y, sizeof(uint8_t));
    map[22] = 1; // Only test REMOVE
    map[68] = 2; // Only test REMOVE
    map[69] = 2; // Only test REMOVE
    map[122] = 1; // Only test REMOVE

    while (!WindowShouldClose())
    {
        // if (IsWindowResized())
        // {
        draw_map_area(map_size_x, map_size_y, map); // REMOVE THIS
        // show_chat_messages();
        // }
    }
    
    draw_map_area(map_size_x, map_size_y, map); // REMOVE THIS
    show_chat_messages();
    sleep(5);

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
        print_failure("Connection to server failed!\n");
        close_socket(client_tcp_socket);
        CloseWindow();
        exit(1);
    }
}

int register_client()
{
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
    draw_map_area(map_size_x, map_size_y, NULL);
    draw_map_area(map_size_x, map_size_y, NULL); // REMOVE THIS
    
    sleep(10);
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