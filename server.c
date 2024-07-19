#include <stdio.h>
#include "graphical/text_formatter.h"
#include "packets/connection.h"

#define PORT 12345

#ifdef BATTLESHIPS_VERSION
char* APP_VERSION = BATTLESHIPS_VERSION;
#else
char* APP_VERSION = "UNDEFINED!";
#endif

int main()
{
    printf("Welcome to Battleships Server!\n");
    printf("Version: %s\n", APP_VERSION);

    // Call socket creation
    int socket = create_socket();
    if (bind_socket_to_address(socket, PORT) < 0)
    {
        print_failure("Failed to bind socket to address!\n");
        exit(1);
    }
    close_socket(socket);
    printf("Socket closed!\n");
}