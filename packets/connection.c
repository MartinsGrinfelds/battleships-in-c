#include "connection.h"
#include <sys/socket.h> // socket(), AF_INET, SOCK_STREAM
#include <netinet/ip.h> // sockaddr_in struct
#include <stdio.h> // perror()
#include <stdlib.h> // exit()
#include <string.h> // memset()
#include <unistd.h> // close()


int create_socket()
{
    // AF_INET      IPv4 Internet protocols
    int main_tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (main_tcp_socket < 0)
    {
        perror("Error: ");
        exit(1); // Can be return NEGATIVE
    }
    printf("Main socket created!\n");
    return main_tcp_socket;
}

int bind_socket_to_address(int socket, uint16_t port)
{
    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address)); // Clean possible garbage from this
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(port);
    int bint_return = bind(socket, (struct sockaddr *)&server_address, sizeof(server_address));

    if (bint_return < 0)
    {
        perror("Error: ");
    }
    printf("Main socket binded!\n");
    return bint_return;
}

void close_socket(int socket)
{
    close(socket);
}
