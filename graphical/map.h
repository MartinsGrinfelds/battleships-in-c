#pragma once

#include <stdint.h> // uint definitions
#include "../packets/shared_packets.h"

/// @brief Takes StatePacket and fills map with objects.
/// @param state Current state of game
/// @param map Pointer to map (can be NULL then will be initialized)
/// @param allied_team Current team id to mark other team ships as enemies
void update_map_with_objects(struct StatePacket *state, uint8_t *map, uint8_t allied_team);

/// @brief Gets map index for 1D array representation.
/// @param width Map width
/// @param height Map height
/// @param x Current x coordinate
/// @param y Current y coordinate
/// @return Your index for 1D map (Enjoy!!!)
size_t get_map_index(size_t width, size_t height, size_t x, size_t y);