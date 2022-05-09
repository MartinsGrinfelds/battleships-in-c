#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include "../helpers/packet_types.h"
#include "../helpers/packet_utils.h"
#include "server_functions.h"

/**************** MAIN *****************/

int main()
{
    

    
    int pid = 0;
    //int i;
    //printf("SERVER started!\n");
    get_shared_memory();
    //pid = fork();

    //if (pid == 0)
    //{
       // gameloop();
    //}
    //else
    //{

            start_game();


        
    
        
    //}

    return 0;
}

/* TODO: Create loop */