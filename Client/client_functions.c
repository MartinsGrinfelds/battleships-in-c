#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ncurses.h>
#include <pthread.h>
#include "../helpers/packet_types.h"
#include "../helpers/packet_utils.h"

void process_packet_client(void *packet)
{
    struct GENERIC_PACKET *packet_template = (struct GENERIC_PACKET *)packet;

    if (packet_template->packet_type == 1) {
        char inputs[32];
        scanf("%s", inputs);
        if (strcmp(inputs, "ready") == 0 ) {
            printf("Test worked!\n");
        }
        
    }

//     switch (expression)
// ​    {
//     case constant1:
//       // statements
//       break;

//     case constant2:
//       // statements
//       break;

//     default:
//       // default statements
//     }


    // LOGIC -> if the packet sent denotes an action required by client -> draw + user input, else just draw and wait for next
}
void process_user_input()
{
}

void *myThreadFunction(void *vargp)
{
    sleep(1);
    printf("Printing from Thread \n");
    return NULL;
}

void ncurses_test()
{

    int x = 0, y = 0;
    int max_x, max_y;
    WINDOW *top;
    WINDOW *bottom;

    pthread_t thread_id;
    printf("Before Thread\n");
    pthread_create(&thread_id, NULL, myThreadFunction, NULL);
    pthread_join(thread_id, NULL);
    printf("After Thread\n");

    initscr();

    //raw();
    keypad(stdscr, TRUE);

    getmaxyx(stdscr, max_y, max_x);

    top = newwin(max_y / 1.2, max_x, 0, 0);
    bottom = newwin(max_y / 6, max_x, max_y / 1.2, 0);

    box(top, 0, 0);
    box(bottom, 0, 0);

    wsetscrreg(top, 1, max_y / 2 - 2);
    wsetscrreg(bottom, 1, max_y / 2 - 2);

    int red_team_player_count = 0;
    char red_team[16][64];
    strcpy(red_team[0], "KASPARS");
    strcpy(red_team[1], "ne-KASPARS");
    strcpy(red_team[2], "daiga");


    int blue_team_player_count = 0;
    char blue_team[16][64];
    strcpy(blue_team[0], "mezonis");
    strcpy(blue_team[1], "darbinieks");
    strcpy(blue_team[2], "darznieks");
    

    int input = 3;
    noecho();
    curs_set(FALSE);

    char str[100];
    while (1)
    {
        //clear(); // Clear the screen of all
        //  previously-printed characters
    

        werase(top);
        werase(bottom);

        box(top, 0, 0);
        box(bottom, 0, 0);

        mvwprintw(top, 1, 2, "Please enter your handle:\n"); 
        wrefresh(top);



        
        mvwgetstr(bottom, input, 2, str);
        
        mvwprintw(bottom, input+1, 2, str); 
        wrefresh(bottom);    
        
        
        usleep(1000000); 
        // refresh();
        // int key_pressed = getch();

        // if (key_pressed == KEY_ENTER) {
        //     //wrefresh(top);
        // wrefresh(bottom);
        // }
         // Shorter delay between movements
                // Advance the ball to the right
    }

    endwin(); // Restore normal terminal behavior
}
