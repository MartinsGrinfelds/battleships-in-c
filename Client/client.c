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

    char *servername;
    struct sockaddr_in remote_address;
    
    char inputs[100];
    char buffer[4096];

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
            
            struct HELLO helloP; 
            helloP.player_name_length = 8;
            strcpy(helloP.player_name, "Kaspars");

            struct GENERIC_PACKET testP;
            testP.sequence_number = 10;
            testP.packet_content_size = 9;
            testP.packet_type = 5;
            testP.checksum = 7;
            testP.content[0] = *(char*)(void*)&helloP;
            

            strcpy(buffer, encode((void*) &testP, (uint8_t) sizeof(testP)));
            print_bytes((void*)&buffer,2050);
            send(my_socket, buffer, strlen(buffer), 0);
            send(my_socket, "\0", 1, 0); // Sending binary zero
            send(my_socket, "\0", 1, 0); // As an end of packet
        }
    }
    return 0;
}