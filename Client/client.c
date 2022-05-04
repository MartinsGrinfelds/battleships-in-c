#include<stdio.h>
#include<sys/socket.h>
#include<string.h>
#include<arpa/inet.h>
#include<unistd.h>
#include"../helpers/packet_types.c"

#define HOST "127.0.0.1"
#define PORT 12345

int main() {
    int my_socket = 0;

    char *servername;
    struct sockaddr_in remote_address;
    
    char inputs[100];

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
            send(my_socket, inputs, strlen(inputs), 0);
            send(my_socket, "\0", 1, 0); // Sending binary zero
            send(my_socket, "\0", 1, 0); // As an end of packet
        }
    }
    return 0;
}