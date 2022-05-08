#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ncurses.h>
#include <pthread.h>
#include "../helpers/packet_types.h"
#include "../helpers/packet_utils.h"

WINDOW *top;
WINDOW *bottom;

int pid = 0;
int game_screen_state = 0;

void send_hello_packet(int socket, char* str) {
    char buffer[DOUBLE_OUT];

        //scanf("%s", inputs);
    struct HELLO helloP;
    struct GENERIC_PACKET testP;
    // print_bytes((void*)&buffer, sizeof(buffer));

    memset(helloP.player_name, 0, sizeof(helloP.player_name));
    
    strcpy(helloP.player_name, str);
    helloP.player_name_length = strlen(str);

    testP.sequence_number = 10;
    testP.packet_content_size = 0;
    testP.packet_type = 0;

    // testP.content[0] = *(char*)(void*)&helloP;
    memset(testP.content, 0, sizeof(testP.content));
    memcpy(testP.content, &helloP, sizeof(helloP));

    testP.checksum = get_checksum(testP);
    //memset(buffer, 0, sizeof(buffer));
    // print_bytes((void*)&testP, sizeof(testP));
    //printf("Double out: %d\nPacket size: %d\n", DOUBLE_OUT, sizeof(testP));
    size_t encoded_length = 0;
    void* encoded_packet = encode((void *)&testP, sizeof(testP), &encoded_length); // free encoded_packet?
    memcpy(buffer, encoded_packet, encoded_length);
    //print_bytes(buffer, sizeof(testP)+1);
    print_bytes(buffer, 15);
    send(socket, "\0", 1, 0); // Sending binary zero
    send(socket, "\0", 1, 0); // As an end of packet
    send(socket, buffer, encoded_length, 0);
    send(socket, "\0", 1, 0); // Sending binary zero
    send(socket, "\0", 1, 0); // As an end of packet

}

void start_ncurses(int socket) {

    int x = 0, y = 0;
    int max_x, max_y;


    initscr();

    keypad(stdscr, TRUE);

    getmaxyx(stdscr, max_y, max_x);

    top = newwin(max_y / 1.2, max_x, 0, 0);
    bottom = newwin(max_y / 6, max_x, max_y / 1.2, 0);

    box(top, 0, 0);
    box(bottom, 0, 0);

    wsetscrreg(top, 1, max_y / 2 - 2);
    wsetscrreg(bottom, 1, max_y / 2 - 2);

}

void handle_second_screen(int socket) { // This is for user input through the bottom window.

    int input = 3;
    int hello_packet_sent = 0;
    int ready_packet_sent = 0;

    char str[100];
    while (1)
    {
        werase(bottom);
        box(bottom, 0, 0);

        switch (game_screen_state)     //NOT 2 ,4 , 8, 11
        { 
            case 0:
            printf("Pre-Lobby!\n");
            
            mvwgetstr(bottom, input, 2, str);
        
            mvwprintw(bottom, input+1, 2, str); 
            wrefresh(bottom);  
            game_screen_state++;

            break;
            default:
            break;
        }




        if (hello_packet_sent == 0) 
        {
            printf("%d <- hello packet too much?\n", hello_packet_sent);
            
            send_hello_packet(socket, str);
            hello_packet_sent++;
            
        }  

         usleep(30000); 

    }

    endwin(); // Restore normal terminal behavior
}

void process_packet_client(int socket, void *packet)
{
    printf("Entering process_packet_client\n");
    struct GENERIC_PACKET *packet_template = (struct GENERIC_PACKET *)packet;

    int received_packet_type = packet_template->packet_type;

    switch (received_packet_type)     //NOT 2 ,4 , 8, 11
    { 
    case 1:
      printf("ACK received!\n");
      break;
    case 3:
      printf("Lobby status received!\n");
      break;
    case 5:
      printf("START_SETUP received!\n");
      break;
    case 6:
      printf("STATE received!\n");
      break;
    case 7:
      printf("TEV Jaliek received!\n");
      break;
    case 9:
      printf("START_GAME received!\n");
      break;
    case 10:
      printf("TEV Jaiet received!\n");
      break;
    case 12:
      printf("END_GAME received!\n");
      break;
    default:
      printf("There shouldn't be anything here. Wrong packet type?\n");
      break;
    }

}

void draw_first_screen(char* data_to_draw, void* packet) { // This is for drawing what server sends.
    // A switch for different states to be drawn.

}

void process_user_input(int socket)
{
    handle_second_screen(socket);
}

void client_gameloop(int socket) {

    pid = fork();

    if (pid == 0)
    {
        process_incoming_packet(socket, 0, 0);
    }
    else
    {
        process_user_input(socket);
    }

}

void handle_pre_lobby(int socket) {}
void handle_lobby() {}
void handle_setup() {}
void handle_game() {}






