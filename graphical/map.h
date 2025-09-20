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
#define PLACED_SHIP 6 // Temporarily placed ship (not confirmed by server yet)
#define PLACED_SHIP_TAIL 7

// Configuration
#define VISIBLE_RANGE 10 // How far can ships see

/// @brief Adds object to map based on index, rotation and type.
/// @param map Pointer to map (must be initialized).
/// @param width Map width.
/// @param index Index on map to add object (0 based).
/// @param rotation Rotation of object (0 - Up, 1 - Right, 2 - Down, 3 - Left).
/// @param object_type Type of object
/// @param ship_id ID to mark ship on map (for example ALLIED_SHIP, ENEMY_SHIP, PLACED_SHIP).
int add_object(uint8_t *map, uint8_t width, size_t index, uint8_t rotation, uint8_t object_type, uint8_t ship_id);

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

/// @brief Gets map index from absolute X, Y position.
/// @param width Map width
/// @param absolute_x Absolute X position
/// @param absolute_y Absolute Y position
/// @return Your index for 1D map. If -1 then out of bounds.
int get_map_index_from_absolute(size_t width, float absolute_x, float absolute_y);
