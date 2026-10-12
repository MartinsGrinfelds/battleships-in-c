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
#define PLACED_SHIP_COLOR \
    (Color) { 189, 171, 174, 255 }
#define TEAMMATE_COLOR \
    (Color) { 160, 200, 170, 255 }
#define SUNK_COLOR \
    (Color) { 45, 46, 47, 255 }
#define ZONE_COLOR \
    (Color) { 30, 120, 150, 255 }
#define INVALID_COLOR \
    (Color) { 235, 92, 109, 160 }
#define HIT_COLOR \
    (Color) { 255, 200, 40, 255 }
#define MISS_COLOR \
    (Color) { 200, 220, 240, 200 }
#define BOW_COLOR \
    (Color) { 14, 14, 14, 140 }
#define SELECTED_COLOR \
    (Color) { 255, 230, 120, 255 }

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

/// @brief Loads font with Latvian letters for title and dedication. Call after window is created.
void load_ui_fonts();

/// @brief Unloads fonts loaded by load_ui_fonts().
void unload_ui_fonts();

/// @brief Draws map area on the screen. Dynamically calculates each square size based on windows size.
/// @param width Map max x coordinate (count starts from 1).
/// @param height Map max y coordinate (count starts from 1).
/// @param map Map data as a list with horizontal scan mode (y * width + x).
void draw_map_area(uint8_t width, uint8_t height, uint8_t *map);

/// @brief Get user input from UI.
/// @param min Min allowed username length.
/// @param max Max allowed username length.
/// @param message Message to show user on what to enter.
/// @param server Server address shown to user.
/// @return Pointer to username string (created by calloc). NULL if window was closed.
char *get_username_input(uint8_t min, uint8_t max, char *message, const char *server);

/// @brief Draws title screen text with dedication (used on start screens).
void draw_dedication(int y);

/// @brief Shows one centered message on whole window (for example "Connecting...").
void draw_center_message(const char *title, const char *message);

/// @brief Draws chat area on the screen.
/// @param messages Linked list of messages that can imidiately be displayed.
/// @param input_line Chat input line (or hint) shown at the bottom.
/// @param input_active Is player typing message now.
/// Should go through messages that are linked list:
/// sender_name (ends with /0), receiver_name (ends with /0), message (ends with /0), next_message (pointer)
void show_chat_messages(struct Message* messages, const char *input_line, int input_active);

/// @brief Displays game info at the right side of the window.
/// @param player_name Current player name.
/// @param player_id Current player ID.
/// @param team_id Current player team ID.
/// @param state Game state containing info about players.
/// @param game_message Message about game (more from server).
/// @param action_message Message about what is happening now (player selected action or what to do).
/// @param timer_message Message about time left.
/// @param ships_text Info about own ships (can be NULL).
void draw_status_area(char* player_name, uint8_t player_id, uint8_t team_id, struct StatePacket *state, char *game_message,
                      char* action_message, char *timer_message, char *ships_text);

/// @brief Gets info as text about players and their team IDs
/// @param state Game state.
/// @param player_id Current player (marked as "You").
/// @return Text about players. FREE AFTER USE!!!
char *get_player_info(struct StatePacket *state, uint8_t player_id); // TODO: not quite UI function, investigate.

/// @brief Shows game result on top of map (INTR-0029).
void draw_end_game_overlay(uint8_t width, uint8_t height, const char *title, const char *subtitle);

/// @brief Draws text with word wrap.
/// @return Height of drawn text in pixels.
int draw_wrapped_text(const char *text, int x, int y, int width, int font_size, uint32_t color_rgba);
