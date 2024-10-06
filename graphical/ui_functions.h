#pragma once
#include <stdint.h> // uint definitions
#include "../packets/shared_packets.h"

// Color definitions for map.
// Ideas from book "Palette Perfect for Graphic Designers & Illustrators... " by Sara Caldas.
// Page 192. -> Sea FOW (31, 56, 104), Sea close (23, 98, 159), Ship (229, 221, 205), Enemy (235, 92, 109)
// Page 259. -> (14, 14, 14), FOW close squares (45, 46, 47), FOW? (114, 106, 106), Ship (189, 171, 174)
#define SHIP_COLOR \
    (Color) { 229, 221, 205, 255 }
#define FOW_SEA_COLOR \
    (Color) { 31, 56, 104, 255 }
#define SEA_COLOR \
    (Color) { 23, 98, 159, 255 }
#define ENEMY_COLOR \
    (Color) { 235, 92, 109, 255 }

struct Message
{
    char *sender_name;
    char *receiver_name;
    char *message;
    void *next_message;
};

/// @brief Gets user input. Pointer must be freed after.
/// @param min Minimum user input length.
/// @param min Maximum user input length.
/// @return Pointer to input string (input length + \0).
char *get_user_input(uint16_t min, uint16_t max);

/// @brief Draws map area on the screen. Dynamically calculates each square size based on windows size.
/// @param width Map max x coordinate (count starts from 1).
/// @param height Map max y coordinate (count starts from 1).
/// @param map Map data as a list with vertical scan mode.
void draw_map_area(uint8_t width, uint8_t height, uint8_t *map);

/// @brief Get user input from UI.
/// @param min Min allowed username length.
/// @param max Max allowed username length.
/// @param message Message to show user on what to enter.
/// @return Pointer to username string (created by calloc).
char *get_username_input(uint8_t min, uint8_t max, char *message);

/// @brief Draws chat area on the screen.
/// @param messages Linked list of messages that can imidiately be displayed.
/// Should go through messages that are linked list:
/// sender_name (ends with /0), receiver_name (ends with /0), message (ends with /0), next_message (pointer)
void show_chat_messages(struct Message* messages);

/// @brief Displays game info at the right side of the window.
/// @param player_name Current player name.
/// @param player_id Current player ID.
/// @param team_id Current player team ID.
/// @param state Game state containing info about players.
/// @param game_message Message about game (more from server).
/// @param action_message Message about what is happening now (player selected action or what to do).
void draw_status_area(char* player_name, uint8_t player_id, uint8_t team_id, struct StatePacket *state, char *game_message, char* action_message);

/// @brief Gets info as text about players and their team IDs
/// @param state Game state.
/// @return Text about players
char *get_player_info(struct StatePacket *state); // TODO: not quite UI function, investigate.