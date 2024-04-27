#pragma once
#include<ncurses.h>

#define HOST "127.0.0.1"
#define PORT 12345

void process_packet_client(int socket, void* packet);
void process_user_input();
void ncurses_test(int socket);
void start_ncurses(int socket);
void send_hello_packet(int socket, char* player_name);
void client_gameloop(int socket);
void handle_second_screen(int socket);
void draw_first_screen(char* data_to_draw, void* packet);
void client_gameloop(int socket);
void process_user_input(int socket);

void handle_pre_lobby(int socket);
void handle_lobby();
void handle_setup();
void handle_game();