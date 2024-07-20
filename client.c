#include <stdio.h>

#define HOST "127.0.0.1"
#define PORT 12345

#ifdef BATTLESHIPS_VERSION
char* APP_VERSION = BATTLESHIPS_VERSION;
#else
char* APP_VERSION = "UNDEFINED!";
#endif

int client_tcp_socket = -1;

/// @brief Does client startup actions such as socket creation, binding, connection
void startup_client()
{
    // Call socket creation
    client_tcp_socket = create_socket();
    // if (bind_socket_to_address(server_tcp_socket, PORT) < 0)
    // {
    //     print_failure("Failed to bind socket to address!\n");
    //     exit(1);
    // }
    // if (listen_to_socket(server_tcp_socket, MAX_REQUESTS) < 0)
    // {
    //     print_failure("Failed to listen on socket!\n");
    //     exit(2);
    // }
}

void main()
{
    printf("Welcome to Battleships Client!\n");
    printf("Version: %s\n", APP_VERSION);

}