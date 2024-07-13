#include <stdio.h>
#include <unistd.h> // close(), etc.
#include <sys/socket.h> // socket(). etc.
#include <netinet/ip.h> // sockaddr_in struct
#include <string.h> // memset()
#define MAX_CLIENTS 10
#define PORT 12345

void print_error(char * err_msg)
{
    printf("\033[91m%s\033[0m", err_msg);
}

int main()
{
    // AF_INET      IPv4 Internet protocols
    int main_tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (main_tcp_socket < 0)
    {
        perror("Error: ");
        print_error("Failure to create a socket!\n");
        return 1;
    }
    printf("Main socket created!\n");

    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address)); // Clean possible garbage from this
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(PORT);

    if (bind(main_tcp_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
    {
        perror("Error: ");
        print_error("Failure to assign address to socket!\n");
        return 2;
    }
    printf("Main socket binded!\n");

    if (listen(main_tcp_socket, MAX_CLIENTS) < 0)
    {
        perror("Error: ");
        print_error("Failure to start listening on socket!\n");
        return 3;
    }
    printf("Main socket is listening!\n");

    struct sockaddr_in client_address;
    memset(&client_address, 0, sizeof(client_address)); // Clean possible garbage from this
    socklen_t client_len = sizeof(client_address);
    int client_socket = accept(main_tcp_socket, (struct sockaddr *)&client_address, &client_len);
    if (client_socket < 0)
    {
        print_error("Mistique error when accepting client connection!");
    }
    
    char reading;
    printf("Receiving from client:\n");
    while (recv(client_socket, &reading, 1, 0) > 0) {
        // buffer[bytes_read] = '\0'; // Null-terminate the buffer
        printf("%c", reading);
    }

    close(client_socket);
    close(main_tcp_socket);
}