#pragma once
#define MAX_CLIENTS 16
#define PORT 12345
#define SHARED_MEMORY_SIZE 1024 * 1024

void get_shared_memory();

void gameloop();

void process_packet_server(void* packet);

void start_network();