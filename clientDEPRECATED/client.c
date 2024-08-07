#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "../packets/packet_types.h"
#include "../packets/packet_utils.h"
#include "client_functions.h"

int main()
{
    //ncurses_test();
    int my_socket = 0;

    char *servername;
    struct sockaddr_in remote_address;

    char inputs[100];
    
    char in[1];

    remote_address.sin_family = AF_INET;
    remote_address.sin_port = htons(PORT);

    // servername = gethostname(HOST);
    inet_pton(AF_INET, HOST, &remote_address.sin_addr);

    if ((my_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("SOCKET ERROR\n");
        return -1;
    }

    if (connect(my_socket, (struct sockaddr *)&remote_address, sizeof(remote_address)) < 0)
    {
        printf("ERROR when making a connection.\n");
        return -1;
    }
    else
    {


        //pid = fork();
        client_gameloop(my_socket);


        sleep(10);

    }
    return 0;
}
