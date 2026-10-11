#pragma once
#include <stdint.h> // uint definitions
#include <stddef.h> // size_t
#include "../packets/shared_packets.h" // MapObject

// Game rules shared by server (to validate) and client (to show what is possible).
// Nothing here uses network or graphics so it can be tested on its own.

// Object types (wiki: Object Types)
#define OBJECT_BASE_SHIP 1
#define OBJECT_BATTLESHIP 2
#define OBJECT_SUBMARINE 3
#define OBJECT_SMALL_SHIP 4
#define OBJECT_FAST_BOAT 5
#define OBJECT_MISSED_HIT 13 // Shown only to team that fired.
#define OBJECT_HIT_SQUARE 14 // Shown only to team that fired.

// Rotation (wiki: Rotation). Same values are used as move direction.
#define ROTATION_UP 0
#define ROTATION_RIGHT 1
#define ROTATION_DOWN 2
#define ROTATION_LEFT 3

#define RANGE_INFINITE -1
#define RANGE_STRAIGHT_LINE -2

#define MIN_TEAMS 2
#define MAX_TEAMS 4
#define MAX_TEAM_ZONE_SIZE 20 // Each team places ships in its own map corner.
#define MAX_SHIP_LENGTH 5

// Results of validation. 0 means that action is allowed.
#define RULE_OK 0
#define RULE_INVALID_SHIP_TYPE 1
#define RULE_INVALID_ROTATION 2
#define RULE_OUT_OF_MAP 3
#define RULE_OUT_OF_ZONE 4
#define RULE_OCCUPIED 5
#define RULE_SHIP_CANNOT_MOVE 6
#define RULE_SHIP_DAMAGED 7
#define RULE_NO_MOVES_LEFT 8
#define RULE_CANNOT_TURN_AROUND 9
#define RULE_SHIP_CANNOT_SHOOT 10
#define RULE_OUT_OF_RANGE 11
#define RULE_NO_SHOTS_LEFT 12
#define RULE_FRIENDLY_FIRE 13
#define RULE_SHIP_SUNK 14
#define RULE_NOT_YOUR_TURN 15
#define RULE_NO_SUCH_SHIP 16

// Shot results.
#define SHOT_MISS 0
#define SHOT_HIT 1
#define SHOT_SUNK 2
#define SHOT_ALREADY_HIT 3

/// @brief Ship properties (wiki: Ship Properties).
struct ShipProperties
{
    const char *name;
    uint8_t speed; // Moves per turn.
    int range; // Shooting range in squares, RANGE_INFINITE or RANGE_STRAIGHT_LINE. 0 means can't shoot.
    uint8_t length;
    uint8_t vision; // How far ship sees (fog of war).
};

/// @brief Gets properties of ship type.
/// @return Properties or NULL if object type is not a ship.
const struct ShipProperties *get_ship_properties(uint8_t object_type);

/// @brief Is object type a ship (1..5).
int is_ship(uint8_t object_type);

/// @brief Gets X and Y change for rotation/direction.
void direction_delta(uint8_t direction, int *dx, int *dy);

/// @brief Calculates squares covered by ship. Index 0 is tail (x, y), last is head.
/// @return Ship length (count of squares) or 0 if object is not a ship.
int ship_squares(const struct MapObject *ship, int xs[MAX_SHIP_LENGTH], int ys[MAX_SHIP_LENGTH]);

/// @brief Checks if ship occupies square.
/// @return Segment index (0 is tail) or -1 if square is not occupied by ship.
int ship_segment_at(const struct MapObject *ship, int x, int y);

/// @brief Is every ship segment damaged.
int ship_is_sunk(const struct MapObject *ship);

/// @brief Is any ship segment damaged.
int ship_is_damaged(const struct MapObject *ship);

/// @brief Count of ship segments that are not damaged.
int ship_healthy_segments(const struct MapObject *ship);

/// @brief Gets corner zone where team places ships.
/// Team 1 - top left, 2 - bottom right, 3 - top right, 4 - bottom left.
/// @return 0 on success, -1 if team is not valid.
int team_zone(uint8_t team_id, uint8_t map_width, uint8_t map_height, int *x0, int *y0, int *x1, int *y1);

/// @brief Is square inside of team placement zone.
int in_team_zone(uint8_t team_id, uint8_t map_width, uint8_t map_height, int x, int y);

/// @brief Is whole ship inside map.
int ship_in_map(const struct MapObject *ship, uint8_t map_width, uint8_t map_height);

/// @brief Does any square of ship overlap with any not sunk ship (except ignored one).
/// @param ignore Ship to ignore (used when ship moves). Can be pointer into objects or identical copy. Can be NULL.
int ship_collides(const struct MapObject *ship, const struct MapObject *objects, size_t object_count, const struct MapObject *ignore);

/// @brief Validates ship placement (INTR-0008, INTR-0009).
/// @return RULE_OK or error.
int validate_placement(const struct MapObject *ship, const struct MapObject *objects, size_t object_count, uint8_t map_width, uint8_t map_height);

/// @brief Finds random valid placement within team zone.
/// @param ship Ship with object_type, team_id and owner_id filled. Position and rotation are filled in.
/// @param random_function Function returning random numbers (for example rand).
/// @return RULE_OK on success.
int random_placement(struct MapObject *ship, const struct MapObject *objects, size_t object_count, uint8_t map_width, uint8_t map_height, int (*random_function)(void));

/// @brief Tries to move ship one step (INTR-0015). Ship moves forward if direction is same as rotation,
/// turns 90 degrees (around tail) if direction is to a side and can't turn around 180 degrees.
/// @param ship Ship to move (changed only if move is allowed).
/// @return RULE_OK or error.
int move_ship(struct MapObject *ship, uint8_t direction, const struct MapObject *objects, size_t object_count, uint8_t map_width, uint8_t map_height);

/// @brief Checks if ship can shoot at square (INTR-0016).
/// @return RULE_OK or error.
int validate_shot(const struct MapObject *ship, int x, int y, uint8_t map_width, uint8_t map_height);

/// @brief Applies shot to all ships. Friendly fire must be validated before.
/// @param hit_ship_index Index of hit ship or -1.
/// @return SHOT_MISS, SHOT_HIT, SHOT_SUNK or SHOT_ALREADY_HIT.
int apply_shot(struct MapObject *objects, size_t object_count, int x, int y, int *hit_ship_index);

/// @brief Is there not sunk ship of team at square.
int team_ship_at(const struct MapObject *objects, size_t object_count, uint8_t team_id, int x, int y);

/// @brief Does team see object (fog of war). Observers (team 0) see everything.
int team_sees_object(uint8_t team_id, const struct MapObject *object, const struct MapObject *objects, size_t object_count);

/// @brief Does team see square (fog of war).
int team_sees_square(uint8_t team_id, int x, int y, const struct MapObject *objects, size_t object_count);

/// @brief Count of not sunk ships of team.
int count_alive_ships(const struct MapObject *objects, size_t object_count, uint8_t team_id);

/// @brief Count of not damaged segments of all team ships (used for scoring).
int count_healthy_segments(const struct MapObject *objects, size_t object_count, uint8_t team_id);

/// @brief Human readable description of rule result. Words start with capital letter.
const char *rule_text(int rule_result);
