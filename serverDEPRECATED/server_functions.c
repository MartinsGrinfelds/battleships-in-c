#include <stdio.h>
#include <unistd.h>
#include <stdlib.h> // size_t
#include <sys/socket.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include "../packets/packet_types.h"
#include "../packets/packet_utils.h"
#include "server_functions.h"

int main_socket;
struct sockaddr_in server_address;
int client_socket;
struct sockaddr_in client_address;
int client_address_size = sizeof(client_address);


uint8_t *kill_gameloop = NULL;
char *shared_memory = NULL;
int client_count = 0;
int *shared_data = NULL;
struct STATE *state = NULL;
struct Lobby *lobby = NULL;
int game_state = 0;
int *player_sockets = NULL;

void get_shared_memory()
{
    kill_gameloop = (uint8_t *)mmap(NULL, 1, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *kill_gameloop = 0;
    shared_memory = mmap(NULL, SHARED_MEMORY_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    // client_count = (int *)shared_memory;
    shared_data = (int *)shared_memory;
    state = (struct STATE *)(shared_memory + sizeof(int));
    lobby = (struct Lobby *)(shared_memory + sizeof(int) + sizeof(struct STATE));
    player_sockets = (int *)(shared_memory + sizeof(int) + sizeof(struct STATE) + sizeof(struct Lobby));
    lobby->player_count = 0;
    int team = 1;
    for (size_t i = 0; i < MAX_CLIENTS; i++)
    {
        lobby->players[i].player_ID = i + 1;
        lobby->players[i].team_ID = team;
        lobby->players[i].ready = 2;
        lobby->players[i].player_name_length = 0;
        // printf("Name: %s\n",lobby.players[i].player_name);
        // strcpy(lobby.players[i].player_name,"");
        team = -team;
        player_sockets[i] = -1;
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
                    if (lobby->players[inner_id].ready == 2)
                    {
                        inner_id--;
                        break;
                    }
                }
                if (i == inner_id)
                {
                    lobby->players[i].ready = 2;
                    player_sockets[i] = -1;
                }
                else
                {
                    strcpy(lobby->players[i].player_name, lobby->players[inner_id].player_name);
                    player_sockets[i] = player_sockets[inner_id];
                    lobby->players[inner_id].ready = 2;
                }
            }
        }
    }
}

void gameloop()
{
    //printf("Starting...\n");

    int i = 0;
    char buffer[DOUBLE_OUT];
    while (1)
    {
        if (*kill_gameloop)
        {
            return;
        }
        
        switch (game_state)
        {
        case 0:
        {
            for (size_t i = 0; i < MAX_CLIENTS; i++)
            {
                int player_socket = player_sockets[i];
                if (player_socket < 0)
                {
                    break;
                }

                    struct GenericPacket packet_to_send;
                packet_to_send.packet_type = 3;
                memcpy(packet_to_send.content, &lobby, sizeof(struct Lobby));
                packet_to_send.checksum = get_checksum(packet_to_send);
                size_t encoded_length = 0;
                void *encoded_packet = encode((void *)&packet_to_send, sizeof(packet_to_send), &encoded_length); // free encoded_packet?
                memcpy(buffer, encoded_packet, encoded_length);
                send(player_socket, "\0", 1, 0); // Sending binary zero
                send(player_socket, "\0", 1, 0); // As an end of packet
                send(player_socket, buffer, encoded_length, 0);
                send(player_socket, "\0", 1, 0); // Sending binary zero
                send(player_socket, "\0", 1, 0); // As an end of packet
                
                //printf("Sent to socket: %d Status: %ld\n", player_socket, listen(player_socket, 40));
            }
        }
        break;

        default:
            break;
        }

        sleep(1);
        printf("Loop...\n");
    }
}

void process_packet_server(int socket, void *packet, int client_id)
{
    char buffer[DOUBLE_OUT];
    struct GenericPacket packet_template = *((struct GenericPacket *)packet);

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
            // printf("Lobby: %d\n", lobby->players[i].ready);
            //  lobby->players[i].ready--;
            if (lobby->players[i].ready == 2)
            {
                // printf("Assigning zeroes!\n");

                //*client_id = lobby.players[i].player_ID;
                break;
            }
        }
        if (i == MAX_CLIENTS) // Lobby game state finished
        {
            printf("Maxed out!\n");
            exit(0); // exit or return?
        }
        // printf("Hello packet received!\n");
        struct HELLO hello_template = *(struct HELLO *)(void *)&packet_template.content;
        player_sockets[i] = socket;
        // printf("Player socket: %d\n", socket);
        lobby->players[i].ready = 0;
        strcpy(lobby->players[i].player_name, hello_template.player_name);
        struct ACK ack;
        ack.player_ID = i + 1;
        ack.team_ID = lobby->players[i].team_ID;
        // struct LPlayer player;
        // strcpy(player.player_name, hello_template.player_name);
        struct GenericPacket packet_to_send;
        packet_to_send.packet_type = 1;
        memcpy(packet_to_send.content, &ack, sizeof(ack));
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

void start_game()
{
    int first_time = 1;
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
        int new_client_id = 0;
        int cpid = 0;
        
        client_socket = accept(main_socket, (struct sockaddr *)&client_address, &client_address_size);
        if (client_socket < 0)
        {
            printf("Error! ERRNO=%d\n", errno);
            continue;
        }

        new_client_id = client_count;
        printf("Client count=%d\n", client_count);
        client_count += 1;
        //cpid = ;
        //printf("Gameloop!!!\n");
        *kill_gameloop = (uint8_t)1;
        if (fork() == 0)
        {
            while (*kill_gameloop && !first_time);
            if(first_time)
            {
                *kill_gameloop = 0;
                first_time = 0;
            }
            printf("Gameloop!!!\n");
            gameloop();
            *kill_gameloop = 0;
            exit(0);
            //close(main_socket);
        }
        else
        {
            //printf("Gameloop!!!\n");
            cpid = fork();

            if (cpid == 0)
            {
                process_incoming_packet(client_socket, 1, new_client_id);
                printf("Client %d disconnected or error happened\n", new_client_id);
                remove_player_id(new_client_id);
                exit(0);
            }
            //else
            //{
                //wait(NULL);
                
                
                // client_count -= 1;
                //exit(0);
            //}
            //close(client_socket);
        }
    }
}