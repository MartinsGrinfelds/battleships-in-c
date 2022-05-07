#include<stdio.h>
#include<sys/socket.h>
#include<string.h>
#include<arpa/inet.h>
#include<unistd.h>
#include "../helpers/packet_types.h"
#include "../helpers/packet_utils.h"

#define HOST "127.0.0.1"
#define PORT 12345

int main() {
    int my_socket = 0;
    char buf[4256];

    char *servername;
    struct sockaddr_in remote_address;
    
    char inputs[100];

    struct HELLO hello;
    int player_length = 8;
    hello.player_name_length = player_length;
    strcpy(hello.player_name, "Timothy");

    int hello_size = sizeof(uint8_t) + sizeof(char) * player_length;

    strcpy(buf, encode((void*) &hello, hello_size));
    printf("%s", &buf);

    remote_address.sin_family = AF_INET;
    remote_address.sin_port = htons(PORT);

    // servername = gethostname(HOST);
    inet_pton(AF_INET, HOST, &remote_address.sin_addr);

    if((my_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
        printf("SOCKET ERROR\n");
        return -1;
    }

    if(connect(my_socket, (struct sockaddr *) &remote_address, sizeof(remote_address)) < 0) {
        printf("ERROR when making a connection.\n");
        return -1;
    }
    else {
        while(1) {
            scanf("%s", inputs);
            //send(my_socket, inputs, strlen(inputs), 0);

            send(my_socket, "\0", 1, 0); // Sending binary zero
            send(my_socket, "\0", 1, 0); // As an end of packet

            //send(my_socket, inputs, strlen(inputs), 0);
            send(my_socket, buf, strlen(buf), 0);

            send(my_socket, "\0", 1, 0); // Sending binary zero
            send(my_socket, "\0", 1, 0); // As an end of packet

            send(my_socket, buf, strlen(buf), 0);

            send(my_socket, "\0", 1, 0); // Sending binary zero
            send(my_socket, "\0", 1, 0); // As an end of packet
        }
    }
    return 0;
}