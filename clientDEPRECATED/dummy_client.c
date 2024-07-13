#include <stdio.h>
#include <unistd.h> // close(), etc.
#include <sys/socket.h> // socket(). etc.
#include <netinet/ip.h> // sockaddr_in struct
#include <string.h> // memset()
#include <arpa/inet.h> // inet_pton()
#include <netdb.h> // For getaddrinfo

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
    
    if (inet_pton(AF_INET, SERVER_IP, &server_address.sin_addr) <= 0)
    {
        print_error("Invalid address/ Address not supported");
        return -1;
    }
    // if (bind(main_tcp_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
    // {
    //     perror("Error: ");
    //     print_error("Failure to assign address to socket!\n");
    //     return 2;
    // }
    // printf("Main socket binded!\n");

    if (connect(main_tcp_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
    {
        perror("Error: ");
        print_error("Mistique error when connecting to server!\n");
        return -1;
    }
    char user_input;
    printf("Sending to server!\n");
    while (1)
    {
        user_input = getchar();
        if (user_input == '`')
        {
            break;
        }
        send(main_tcp_socket, &user_input, 1, 0);
    }
    close(main_tcp_socket);
}