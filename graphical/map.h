#pragma once

#include <stdint.h> // uint definitions
#include "../packets/shared_packets.h"

// Definitions for different object IDs on map
#define FOW_SEA 0
#define VISIBLE_SEA 1
#define ALLIED_SHIP 2
#define ALLIED_SHIP_TAIL 3
#define ENEMY_SHIP 4
#define ENEMY_SHIP_TAIL 5

/// @brief Takes StatePacket and fills map with objects.
/// @param state Current state of game
/// @param map Pointer to map (can be NULL then will be initialized)
/// @param allied_team Current team id to mark other team ships as enemies
int update_map_with_objects(struct StatePacket *state, uint8_t *map, uint8_t allied_team);

/// @brief Gets map index for 1D array representation. Index from 0
/// @param width Map width
/// @param x Current x coordinate
/// @param y Current y coordinate
/// @return Your index for 1D map (Enjoy!!!)
size_t get_map_index(size_t width, size_t x, size_t y);