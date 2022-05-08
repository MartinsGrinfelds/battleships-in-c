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

char *shared_memory = NULL;
int client_count = 0;
int *shared_data = NULL;
struct STATE *state = NULL;
struct Lobby *lobby = NULL;
int game_state = 0;

void get_shared_memory()
{
    shared_memory = mmap(NULL, SHARED_MEMORY_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    // client_count = (int *)shared_memory;
    shared_data = (int *)shared_memory;
    state = (struct STATE *)(shared_memory + sizeof(int));
    lobby = (struct Lobby *)(shared_memory + sizeof(int) + sizeof(struct STATE));
    lobby->player_count = 0;
    int team = 1;
    for (size_t i = 0; i <  MAX_CLIENTS; i++)
    {
        lobby->players[i].player_ID = i + 1;
        lobby->players[i].team_ID = team;
        lobby->players[i].ready = 2;
        lobby->players[i].player_name_length = 0;
        // printf("Name: %s\n",lobby.players[i].player_name);
        // strcpy(lobby.players[i].player_name,"");
        team = -team;
        // printf("Team: %d\n", team);
    }
}

void remove_player_id(int player_id)
{
    if (game_state != 0) // What to do if player disconnects mid-game?
    {
        return;
    }
    else
    {
        for (size_t i = 0; i < MAX_CLIENTS; i++)
        {
            if (lobby->players[i].player_ID == player_id)
            {
                int inner_id = i;
                for (; inner_id < MAX_CLIENTS; inner_id++)
                {
                    if (lobby->players[inner_id].ready == -1)
                    {
                        inner_id--;
                        break;
                    }
                }
                if (i == inner_id)
                {
                    lobby->players[i].ready == -1;
                }
                else
                {
                    strcpy(lobby->players[i].player_name, lobby->players[inner_id].player_name);
                }
                return;
            }
            
            
        }
    }
}

void gameloop()
{
    printf("Starting...\n");

    int i = 0;

    while (1)
    {
        switch (game_state)
        {
        case 0:
        {
        }
        break;

        default:
            break;
        }

        // for (i = 0; i < client_count; i++)
        // {
        //     shared_data[MAX_CLIENTS + i] += shared_data[i];
        //     shared_data[i] = 0;
        // }

        sleep(1);
        // printf("Loop...\n");
    }
}

void process_packet_server(int socket, void *packet, int client_id)
{
    char buffer[DOUBLE_OUT];
    struct GENERIC_PACKET packet_template = *((struct GENERIC_PACKET *)packet);

    printf("---START OF PACKET CONTENT---\n");
    printf("Sequence number: %d\n", packet_template.sequence_number);
    printf("Content size: %d\n", packet_template.packet_content_size);
    printf("Packet type: %d\n", packet_template.packet_type);
    printf("Checksum: %d\n", packet_template.checksum);
    printf("---END OF PACKET CONTENT---\n");
    uint8_t packet_checksum = get_checksum(packet_template);
    if (packet_template.checksum != packet_checksum)
    {
        printf("PACKET CORRUPTED!!!\n");
    }

    // else if (packet_template.sequence_number < previous_sequence_number)
    // {
    //     /* code */
    // }
    switch (packet_template.packet_type)
    {
    case 0: // Hello packet
    {
        if (game_state != 0) // Lobby game state finished
        {
            printf("Game state not 0!\n");
            exit(0); // exit or return?
        }
        size_t i = 0;
        for (; i < MAX_CLIENTS; i++)
        {
            printf("Lobby: %d\n", lobby->players[i].ready);
            //lobby->players[i].ready--;
            if (lobby->players[i].ready == 2)
            {
                printf("Assigning zeroes!\n");
                lobby->players[i].ready = 0;
                //*client_id = lobby.players[i].player_ID;
                break;
            }
        }
        if (i == MAX_CLIENTS) // Lobby game state finished
        {
            printf("Maxed out!\n");
            exit(0); // exit or return?
        }
        printf("Hello packet received!\n");
        struct HELLO hello_template = *(struct HELLO *)(void *)&packet_template.content;

        struct LPlayer player;
        strcpy(player.player_name, hello_template.player_name);
        struct GENERIC_PACKET packet_to_send;
        packet_to_send.packet_type = 1;
        packet_to_send.checksum = get_checksum(packet_to_send);
        size_t encoded_length = 0;
        void *encoded_packet = encode((void *)&packet_to_send, sizeof(packet_to_send), &encoded_length); // free encoded_packet?
        memcpy(buffer, encoded_packet, encoded_length);
        send(socket, "\0", 1, 0); // Sending binary zero
        send(socket, "\0", 1, 0); // As an end of packet
        send(socket, buffer, encoded_length, 0);
        send(socket, "\0", 1, 0); // Sending binary zero
        send(socket, "\0", 1, 0); // As an end of packet
    }

    break;
    case 2: // Message
        /* code */
        break;
    case 4: // Client ready
        /* code */
        break;
    case 8: // Client places ship
        /* code */
        break;
    case 11: // Client move type
        /* code */
        break;
    default:
        printf("SOMEONE SENT SH!T TO THE SERVER!\n");
        break;
    }
}

void start_network()
{
    int main_socket;
    struct sockaddr_in server_address;

    int client_socket;
    struct sockaddr_in client_address;
    int client_address_size = sizeof(client_address);

    main_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (main_socket < 0)
    {
        printf("ERROR!\n");
        exit(1);
    }
    printf("Main socket created!\n");

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(PORT);

    if (bind(main_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
    {
        perror("Error: ");
        printf("ERROR2!\n");
        exit(1);
    }
    printf("Main socket binded!\n");

    if (listen(main_socket, MAX_CLIENTS) < 0)
    {
        printf("ERROR3!\n");
        exit(1);
    }
    printf("Main socket is listening!\n");

    while (1)
    {
        int new_client_id = 1;
        int cpid = 0;

        client_socket = accept(main_socket, (struct sockaddr *)&client_address, &client_address_size);
        if (client_socket < 0)
        {
            printf("Error! ERRNO=%d\n", errno);
            continue;
        }

        new_client_id = client_count;
        client_count += 1;
        printf("Client count=%d\n", client_count);
        cpid = fork();

        if (cpid == 0)
        {
            close(main_socket);
            cpid = fork();

            if (cpid == 0)
            {
                process_incoming_packet(client_socket, 1, new_client_id);
                exit(0);
            }
            else
            {
                wait(NULL);
                printf("Orphaning succesful: %d\n", new_client_id);
                // client_count -= 1;
                exit(0);
            }
        }
        else
        {
            //close(client_socket);
        }
    }
}