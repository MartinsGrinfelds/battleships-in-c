#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<sys/wait.h>
#include<arpa/inet.h>
#include<errno.h>
#include<string.h>
#include<sys/mman.h>

#define MAX_CLIENTS 16
#define PORT 12345
#define SHARED_MEMORY_SIZE 1024*1024

char* shared_memory = NULL;
int* client_count = NULL;
int* shared_data = NULL;

void get_shared_memory();
void gameloop();
void start_network();
void process_client(int id, int socket);

int main () {
    int pid = 0;
    int i;
    printf("SERVER started!\n");
    get_shared_memory();
    pid = fork();

    if(pid == 0) {
        start_network();
    }
    else {
        gameloop();
    }

    return 0;
}




/* TODO: Create loop */