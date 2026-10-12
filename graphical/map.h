#pragma once

#include <stdint.h> // uint definitions
#include "../packets/shared_packets.h"

// Definitions for different object IDs on map
#define FOW_SEA 0
#define VISIBLE_SEA 1
#define ALLIED_SHIP 2 // Ship of this player.
#define ALLIED_SHIP_TAIL 3
#define ENEMY_SHIP 4
#define ENEMY_SHIP_TAIL 5
#define PLACED_SHIP 6 // Temporarily placed ship (not confirmed by server yet)
#define PLACED_SHIP_TAIL 7
#define TEAMMATE_SHIP 8 // Ship of other player in same team.
#define TEAMMATE_SHIP_TAIL 9
#define SUNK_SHIP 10
#define INVALID_PLACED_SHIP 11 // Temporarily placed ship that can't be placed there.
#define ZONE_SEA 12 // Team zone where ships can be placed.

// Layout: map at top left, status area on right, chat below map.
#define STATUS_AREA_WIDTH 340
#define CHAT_AREA_HEIGHT 190
#define MAP_MARGIN 8

/// @brief Where map is drawn on screen.
struct MapLayout
{
    int x;
    int y;
    int cell; // Square size in pixels.
    uint8_t width; // Map width in squares.
    uint8_t height; // Map height in squares.
};

/// @brief Calculates square size so that whole map fits in map area and squares stay square.
struct MapLayout get_map_layout(uint8_t width, uint8_t height);

/// @brief Adds ship to map. Squares outside of map are skipped.
/// @param map Pointer to map (must be initialized).
/// @param width Map width.
/// @param height Map height.
/// @param ship Ship to draw.
/// @param ship_id Value for ship squares (for example ALLIED_SHIP, ENEMY_SHIP, PLACED_SHIP).
/// @param tail_id Value for tail square.
/// @return 0 on success.
int add_object(uint8_t *map, uint8_t width, uint8_t height, const struct MapObject *ship, uint8_t ship_id, uint8_t tail_id);

/// @brief Takes StatePacket and fills map with sea, fog of war and ships.
/// @param state Current state of game
/// @param map Pointer to map (width * height bytes)
/// @param allied_team Current team id to mark other team ships as enemies (0 - observer sees everything)
/// @param player_id Current player to mark own ships
int update_map_with_objects(struct StatePacket *state, uint8_t *map, uint8_t allied_team, uint8_t player_id);

/// @brief Gets map index for 1D array representation. Index from 0
/// @param width Map width
/// @param x Current x coordinate
/// @param y Current y coordinate
/// @return Your index for 1D map (Enjoy!!!)
size_t get_map_index(size_t width, size_t x, size_t y);

/// @brief Gets map square from absolute X, Y position (for example mouse).
/// @return 0 if position is on map, -1 if out of bounds.
int get_map_square_from_absolute(uint8_t width, uint8_t height, float absolute_x, float absolute_y, int *x, int *y);

/// @brief Draws things on top of squares: hits (X), misses (dot), ship bows and selected ship outline.
void draw_map_overlays(struct StatePacket *state, uint8_t player_id, uint8_t selected_ship);

/// @brief Draws frame around map square.
void draw_square_highlight(uint8_t width, uint8_t height, int x, int y, int thickness, uint32_t color_rgba);
