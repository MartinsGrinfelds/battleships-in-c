#pragma once
#include<stdint.h>
#include<string.h>
#include<stdio.h>

struct GENERIC_PACKET{
    uint8_t sequence_number;
    uint8_t packet_content_size; /* ?? */
    uint8_t packet_type;
    char* content;   // Kādu izmēru šeit likt?
    uint8_t checksum;
};


struct HELLO {
    uint8_t player_name_length; //??
    char* player_name;
};

struct ACK {
    uint8_t player_ID;
    uint8_t team_ID;
};

struct Message {
    uint8_t message_type;
    uint8_t sender_ID;
    uint8_t recipient_ID;
    uint8_t message_length; // ???
    char* message;
};


struct LPlayer {
    uint8_t player_ID;
    uint8_t team_ID;
    uint8_t ready;
    uint8_t player_name_length; // ??
    char* player_name;
};

struct Lobby {
    uint8_t player_count; // ??
    struct LPlayer* players;
};

struct Ready {
    uint8_t player_ID;
    uint8_t ready;
};

struct Start_setup {
    uint8_t x;
    uint8_t y;
};

struct Ship {
    uint8_t ship_ID;
    uint8_t ship_x;
    uint8_t ship_y;
    uint8_t direction;  // vai šeit jābut Rotation??
    uint8_t ship_team_ID;
    uint8_t damage;
};

struct Player {
    uint8_t player_ID;
    uint8_t team_ID;
    uint8_t ready;
    uint8_t player_name_length; // ?
    char* player_name;
    uint8_t active;
};

struct STATE {
    uint8_t x;
    uint8_t y;
    uint8_t* battlefield; // laukuma izmērs reāli ir x*y
    uint8_t ship_count;
    struct Ship* ships; //ships[ship_count]
    uint8_t player_count;
    struct Player* players;
};

struct Jaliek {
    uint8_t player_ID;
    uint8_t ship_ID;
};

struct Lieku {
    uint8_t player_ID;
    uint8_t ship_ID;
    uint8_t x;
    uint8_t y;
    uint8_t direction;  //Rotation?
};

struct Start_game {
    uint8_t x;
    uint8_t y;
};

struct Jaiet {
    uint8_t player_ID;
    uint8_t ship_ID;
    uint8_t x;
    uint8_t y;
    uint8_t direction;
    uint8_t damage;
};

struct Gajiens{
    uint8_t player_ID;
    uint8_t action;
    uint8_t y;
    uint8_t x;
    uint8_t direction_or_powerup;
};

struct End_game {
    uint8_t winner_ID; //dqf is
    uint8_t winner_team_ID;
};


// int main() {
//     struct HELLO hello;
//     hello.player_name_length = 8;
//     strcpy(hello.player_name, "Timothy");

//     printf("%d, %s\n", hello.player_name_length, hello.player_name);
   
//     struct ACK a_p;
//     a_p.player_ID = 12;
//     a_p.team_ID = 1;

//     printf("%d, %d\n", a_p.player_ID, a_p.team_ID);

//     struct Message msg;

//     return 0;
// }