#pragma once
#include <stdint.h> // uint definitions

// Color definitions for map.
// Ideas from book "Palette Perfect for Graphic Designers & Illustrators... " by Sara Caldas.
// Page 192. -> Sea FOW (31, 56, 104), Sea close (23, 98, 159), Ship (229, 221, 205), Enemy (235, 92, 109)
// Page 259. -> (14, 14, 14), FOW close squares (45, 46, 47), FOW? (114, 106, 106), Ship (189, 171, 174)
#define SHIP_COLOR (Color) {229, 221, 205, 255}
#define FOW_SEA_COLOR (Color) {31, 56, 104, 255}
#define SEA_COLOR (Color) {23, 98, 159, 255}
#define ENEMY_COLOR (Color) {235, 92, 109}

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

/// @brief Get username input from UI.
/// @param max Max allowed username length.
/// @return Pointer to username string (created by calloc).
char* get_username_input(uint8_t max);