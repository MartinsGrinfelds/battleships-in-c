#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ncurses.h>
#include "../helpers/packet_types.h"
#include "../helpers/packet_utils.h"

void process_packet_client(void *packet)
{
    struct GENERIC_PACKET *packet_template = (struct GENERIC_PACKET *)packet;

    if (packet_template->packet_type == 0) {
        char inputs[32];
        scanf("%s", inputs);
        if (strcmp(inputs, "ready") == 0 ) {
            printf("Test worked!\n");
        }
        
    }
}
void process_user_input()
{
}

void ncurses_test()
{

    int x = 0, y = 0;
    int max_x, max_y;
    WINDOW *top;
    WINDOW *bottom;

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

    int input = 1;
    // noecho();
    // curs_set(FALSE);
    char str[100];
    while (1)
    {
        // clear(); // Clear the screen of all
        //  previously-printed characters
        wrefresh(top);
        wrefresh(bottom);
        mvwgetstr(bottom, input, 2, str);
        werase(top);
        box(top, 0, 0);
        mvwprintw(top, 1, 2, str); // Print our "ball" at the current xy position
        
        werase(bottom);
        box(bottom, 0, 0);
        // refresh();
        // int key_pressed = getch();

        // if (key_pressed == KEY_ENTER) {
        //     //wrefresh(top);
        // wrefresh(bottom);
        // }
        usleep(30000); // Shorter delay between movements
        x++;           // Advance the ball to the right
    }

    endwin(); // Restore normal terminal behavior
}
