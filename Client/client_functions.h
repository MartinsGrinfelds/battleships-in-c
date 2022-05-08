#pragma once
#define HOST "127.0.0.1"
#define PORT 12345

void process_packet_client(int socket, void* packet);
void process_user_input();
void ncurses_test(int socket);
void send_hello_packet(int socket, char* player_name);