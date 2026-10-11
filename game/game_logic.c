#include "game_logic.h"
#include <stdlib.h> // abs()
#include <string.h> // memcmp()

// Wiki: Ship Properties (object_type)
static const struct ShipProperties ship_properties[SHIP_TYPE_COUNT] = {
    {"Base Ship", 0, RANGE_INFINITE, 5, 10},
    {"Battleship", 1, 25, 4, 10},
    {"Submarine", 3, RANGE_STRAIGHT_LINE, 3, 10},
    {"Small Ship", 2, 10, 2, 10},
    {"Fast Boat", 5, 0, 1, 15}, // Can't shoot but sees further (intelligence of FOW).
};

const struct ShipProperties *get_ship_properties(uint8_t object_type)
{
    if (!is_ship(object_type))
    {
        return NULL;
    }
    return &ship_properties[object_type - 1];
}

int is_ship(uint8_t object_type)
{
    return object_type >= 1 && object_type <= SHIP_TYPE_COUNT;
}

void direction_delta(uint8_t direction, int *dx, int *dy)
{
    *dx = 0;
    *dy = 0;
    switch (direction)
    {
    case ROTATION_UP:
        *dy = -1;
        break;
    case ROTATION_RIGHT:
        *dx = 1;
        break;
    case ROTATION_DOWN:
        *dy = 1;
        break;
    case ROTATION_LEFT:
        *dx = -1;
        break;
    }
}

int ship_squares(const struct MapObject *ship, int xs[MAX_SHIP_LENGTH], int ys[MAX_SHIP_LENGTH])
{
    const struct ShipProperties *properties = get_ship_properties(ship->object_type);
    if (!properties || ship->rotation > ROTATION_LEFT)
    {
        return 0;
    }
    int dx, dy;
    direction_delta(ship->rotation, &dx, &dy);
    int i = 0;
    while (i < properties->length)
    {
        xs[i] = ship->x + dx * i;
        ys[i] = ship->y + dy * i;
        i++;
    }
    return properties->length;
}

int ship_segment_at(const struct MapObject *ship, int x, int y)
{
    int xs[MAX_SHIP_LENGTH], ys[MAX_SHIP_LENGTH];
    int length = ship_squares(ship, xs, ys);
    int i = 0;
    while (i < length)
    {
        if (xs[i] == x && ys[i] == y)
        {
            return i;
        }
        i++;
    }
    return -1;
}

static uint8_t full_damage_mask(uint8_t object_type)
{
    const struct ShipProperties *properties = get_ship_properties(object_type);
    if (!properties)
    {
        return 0;
    }
    return (uint8_t)((1 << properties->length) - 1);
}

int ship_is_sunk(const struct MapObject *ship)
{
    uint8_t mask = full_damage_mask(ship->object_type);
    return mask && (ship->info & mask) == mask;
}

int ship_is_damaged(const struct MapObject *ship)
{
    return (ship->info & full_damage_mask(ship->object_type)) != 0;
}

int ship_healthy_segments(const struct MapObject *ship)
{
    const struct ShipProperties *properties = get_ship_properties(ship->object_type);
    if (!properties)
    {
        return 0;
    }
    int healthy = 0, i = 0;
    while (i < properties->length)
    {
        if (!(ship->info & (1 << i)))
        {
            healthy++;
        }
        i++;
    }
    return healthy;
}

int team_zone(uint8_t team_id, uint8_t map_width, uint8_t map_height, int *x0, int *y0, int *x1, int *y1)
{
    int size_x = map_width / 2, size_y = map_height / 2;
    if (size_x > MAX_TEAM_ZONE_SIZE)
    {
        size_x = MAX_TEAM_ZONE_SIZE;
    }
    if (size_y > MAX_TEAM_ZONE_SIZE)
    {
        size_y = MAX_TEAM_ZONE_SIZE;
    }
    int left = 0, top = 0, right = map_width - size_x, bottom = map_height - size_y;
    switch (team_id)
    {
    case 1:
        *x0 = left;
        *y0 = top;
        break;
    case 2:
        *x0 = right;
        *y0 = bottom;
        break;
    case 3:
        *x0 = right;
        *y0 = top;
        break;
    case 4:
        *x0 = left;
        *y0 = bottom;
        break;
    default:
        return -1;
    }
    *x1 = *x0 + size_x - 1;
    *y1 = *y0 + size_y - 1;
    return 0;
}

int in_team_zone(uint8_t team_id, uint8_t map_width, uint8_t map_height, int x, int y)
{
    int x0, y0, x1, y1;
    if (team_zone(team_id, map_width, map_height, &x0, &y0, &x1, &y1))
    {
        return 0;
    }
    return x >= x0 && x <= x1 && y >= y0 && y <= y1;
}

int ship_in_map(const struct MapObject *ship, uint8_t map_width, uint8_t map_height)
{
    int xs[MAX_SHIP_LENGTH], ys[MAX_SHIP_LENGTH];
    int length = ship_squares(ship, xs, ys);
    if (length == 0)
    {
        return 0;
    }
    int i = 0;
    while (i < length)
    {
        if (xs[i] < 0 || ys[i] < 0 || xs[i] >= map_width || ys[i] >= map_height)
        {
            return 0;
        }
        i++;
    }
    return 1;
}

int ship_collides(const struct MapObject *ship, const struct MapObject *objects, size_t object_count, const struct MapObject *ignore)
{
    int xs[MAX_SHIP_LENGTH], ys[MAX_SHIP_LENGTH];
    int length = ship_squares(ship, xs, ys);
    size_t object_nr = 0;
    while (object_nr < object_count)
    {
        const struct MapObject *other = &objects[object_nr++];
        // Ship being moved can be a copy (for example when client or bot tries a move first),
        // so same ship is recognised by its values too. Two different ships can't be identical.
        int is_ignored = ignore && (other == ignore || memcmp(other, ignore, sizeof(struct MapObject)) == 0);
        if (is_ignored || !is_ship(other->object_type) || ship_is_sunk(other))
        {
            // Sunk ships go to the bottom and don't block anything.
            continue;
        }
        int i = 0;
        while (i < length)
        {
            if (ship_segment_at(other, xs[i], ys[i]) >= 0)
            {
                return 1;
            }
            i++;
        }
    }
    return 0;
}

int validate_placement(const struct MapObject *ship, const struct MapObject *objects, size_t object_count, uint8_t map_width, uint8_t map_height)
{
    if (!is_ship(ship->object_type))
    {
        return RULE_INVALID_SHIP_TYPE;
    }
    if (ship->rotation > ROTATION_LEFT)
    {
        return RULE_INVALID_ROTATION;
    }
    if (!ship_in_map(ship, map_width, map_height))
    {
        return RULE_OUT_OF_MAP;
    }
    int xs[MAX_SHIP_LENGTH], ys[MAX_SHIP_LENGTH];
    int length = ship_squares(ship, xs, ys);
    int i = 0;
    while (i < length)
    {
        if (!in_team_zone(ship->team_id, map_width, map_height, xs[i], ys[i]))
        {
            return RULE_OUT_OF_ZONE;
        }
        i++;
    }
    if (ship_collides(ship, objects, object_count, NULL))
    {
        return RULE_OCCUPIED;
    }
    return RULE_OK;
}

int random_placement(struct MapObject *ship, const struct MapObject *objects, size_t object_count, uint8_t map_width, uint8_t map_height, int (*random_function)(void))
{
    int x0, y0, x1, y1;
    if (team_zone(ship->team_id, map_width, map_height, &x0, &y0, &x1, &y1))
    {
        return RULE_OUT_OF_ZONE;
    }
    int attempt = 0;
    while (attempt++ < 2000)
    {
        ship->x = (uint8_t)(x0 + random_function() % (x1 - x0 + 1));
        ship->y = (uint8_t)(y0 + random_function() % (y1 - y0 + 1));
        ship->rotation = (uint8_t)(random_function() % 4);
        ship->info = 0;
        if (validate_placement(ship, objects, object_count, map_width, map_height) == RULE_OK)
        {
            return RULE_OK;
        }
    }
    // Zone is very full. Try every option in order.
    int y = y0;
    while (y <= y1)
    {
        int x = x0;
        while (x <= x1)
        {
            uint8_t rotation = 0;
            while (rotation < 4)
            {
                ship->x = (uint8_t)x;
                ship->y = (uint8_t)y;
                ship->rotation = rotation++;
                if (validate_placement(ship, objects, object_count, map_width, map_height) == RULE_OK)
                {
                    return RULE_OK;
                }
            }
            x++;
        }
        y++;
    }
    return RULE_OCCUPIED;
}

int move_ship(struct MapObject *ship, uint8_t direction, const struct MapObject *objects, size_t object_count, uint8_t map_width, uint8_t map_height)
{
    const struct ShipProperties *properties = get_ship_properties(ship->object_type);
    if (!properties)
    {
        return RULE_INVALID_SHIP_TYPE;
    }
    if (direction > ROTATION_LEFT)
    {
        return RULE_INVALID_ROTATION;
    }
    if (ship_is_sunk(ship))
    {
        return RULE_SHIP_SUNK;
    }
    if (properties->speed == 0)
    {
        return RULE_SHIP_CANNOT_MOVE;
    }
    if (ship_is_damaged(ship))
    {
        // Wiki: Damaged ship cannot move.
        return RULE_SHIP_DAMAGED;
    }
    struct MapObject moved = *ship;
    if (direction == ship->rotation)
    {
        // Move forward by one square.
        int dx, dy;
        direction_delta(direction, &dx, &dy);
        int new_x = ship->x + dx, new_y = ship->y + dy;
        if (new_x < 0 || new_y < 0 || new_x >= map_width || new_y >= map_height)
        {
            return RULE_OUT_OF_MAP;
        }
        moved.x = (uint8_t)new_x;
        moved.y = (uint8_t)new_y;
    }
    else if (properties->length > 1 && direction == (ship->rotation + 2) % 4)
    {
        // Wiki: The ship can turn by 90 degrees but cannot move by 180 degrees.
        return RULE_CANNOT_TURN_AROUND;
    }
    else
    {
        // Turn 90 degrees around tail. Fast Boat is one square so it can face any direction.
        moved.rotation = direction;
    }
    if (!ship_in_map(&moved, map_width, map_height))
    {
        return RULE_OUT_OF_MAP;
    }
    if (ship_collides(&moved, objects, object_count, ship))
    {
        // Wiki: Ship cannot move into an occupied field.
        return RULE_OCCUPIED;
    }
    *ship = moved;
    return RULE_OK;
}

int validate_shot(const struct MapObject *ship, int x, int y, uint8_t map_width, uint8_t map_height)
{
    const struct ShipProperties *properties = get_ship_properties(ship->object_type);
    if (!properties)
    {
        return RULE_INVALID_SHIP_TYPE;
    }
    if (ship_is_sunk(ship))
    {
        return RULE_SHIP_SUNK;
    }
    if (x < 0 || y < 0 || x >= map_width || y >= map_height)
    {
        return RULE_OUT_OF_MAP;
    }
    if (properties->range == 0)
    {
        return RULE_SHIP_CANNOT_SHOOT;
    }
    if (properties->range == RANGE_INFINITE)
    {
        return RULE_OK;
    }
    int xs[MAX_SHIP_LENGTH], ys[MAX_SHIP_LENGTH];
    int length = ship_squares(ship, xs, ys);
    int i = 0;
    while (i < length)
    {
        if (properties->range == RANGE_STRAIGHT_LINE)
        {
            // Torpedo goes along row or column of submarine. Any distance.
            if (xs[i] == x || ys[i] == y)
            {
                return RULE_OK;
            }
        }
        else
        {
            int dx = xs[i] - x, dy = ys[i] - y;
            if (dx * dx + dy * dy <= properties->range * properties->range)
            {
                return RULE_OK;
            }
        }
        i++;
    }
    return RULE_OUT_OF_RANGE;
}

int apply_shot(struct MapObject *objects, size_t object_count, int x, int y, int *hit_ship_index)
{
    *hit_ship_index = -1;
    size_t object_nr = 0;
    while (object_nr < object_count)
    {
        struct MapObject *ship = &objects[object_nr];
        if (is_ship(ship->object_type) && !ship_is_sunk(ship))
        {
            int segment = ship_segment_at(ship, x, y);
            if (segment >= 0)
            {
                *hit_ship_index = (int)object_nr;
                if (ship->info & (1 << segment))
                {
                    return SHOT_ALREADY_HIT;
                }
                ship->info |= (uint8_t)(1 << segment);
                return ship_is_sunk(ship) ? SHOT_SUNK : SHOT_HIT;
            }
        }
        object_nr++;
    }
    return SHOT_MISS;
}

int team_ship_at(const struct MapObject *objects, size_t object_count, uint8_t team_id, int x, int y)
{
    size_t object_nr = 0;
    while (object_nr < object_count)
    {
        const struct MapObject *ship = &objects[object_nr++];
        if (is_ship(ship->object_type) && ship->team_id == team_id && !ship_is_sunk(ship) && ship_segment_at(ship, x, y) >= 0)
        {
            return 1;
        }
    }
    return 0;
}

int team_sees_square(uint8_t team_id, int x, int y, const struct MapObject *objects, size_t object_count)
{
    if (team_id == 0)
    {
        return 1;
    }
    size_t object_nr = 0;
    while (object_nr < object_count)
    {
        const struct MapObject *ship = &objects[object_nr++];
        if (!is_ship(ship->object_type) || ship->team_id != team_id || ship_is_sunk(ship))
        {
            continue;
        }
        int vision = get_ship_properties(ship->object_type)->vision;
        int xs[MAX_SHIP_LENGTH], ys[MAX_SHIP_LENGTH];
        int length = ship_squares(ship, xs, ys);
        int i = 0;
        while (i < length)
        {
            // Use circle formula: x^2 + y^2 <= r^2
            int dx = xs[i] - x, dy = ys[i] - y;
            if (dx * dx + dy * dy <= vision * vision)
            {
                return 1;
            }
            i++;
        }
    }
    return 0;
}

int team_sees_object(uint8_t team_id, const struct MapObject *object, const struct MapObject *objects, size_t object_count)
{
    if (team_id == 0 || object->team_id == team_id)
    {
        return 1;
    }
    if (!is_ship(object->object_type))
    {
        return team_sees_square(team_id, object->x, object->y, objects, object_count);
    }
    if (ship_is_sunk(object))
    {
        // Everybody knows where ship went down.
        return 1;
    }
    int xs[MAX_SHIP_LENGTH], ys[MAX_SHIP_LENGTH];
    int length = ship_squares(object, xs, ys);
    int i = 0;
    while (i < length)
    {
        if (team_sees_square(team_id, xs[i], ys[i], objects, object_count))
        {
            return 1;
        }
        i++;
    }
    return 0;
}

int count_alive_ships(const struct MapObject *objects, size_t object_count, uint8_t team_id)
{
    int count = 0;
    size_t object_nr = 0;
    while (object_nr < object_count)
    {
        const struct MapObject *ship = &objects[object_nr++];
        if (is_ship(ship->object_type) && ship->team_id == team_id && !ship_is_sunk(ship))
        {
            count++;
        }
    }
    return count;
}

int count_healthy_segments(const struct MapObject *objects, size_t object_count, uint8_t team_id)
{
    int count = 0;
    size_t object_nr = 0;
    while (object_nr < object_count)
    {
        const struct MapObject *ship = &objects[object_nr++];
        if (is_ship(ship->object_type) && ship->team_id == team_id)
        {
            count += ship_healthy_segments(ship);
        }
    }
    return count;
}

const char *rule_text(int rule_result)
{
    switch (rule_result)
    {
    case RULE_OK:
        return "Ok";
    case RULE_INVALID_SHIP_TYPE:
        return "Invalid Ship Type";
    case RULE_INVALID_ROTATION:
        return "Invalid Rotation";
    case RULE_OUT_OF_MAP:
        return "Ship Must Stay Within Map";
    case RULE_OUT_OF_ZONE:
        return "Ship Must Be Placed In Your Team Zone";
    case RULE_OCCUPIED:
        return "Square Is Occupied";
    case RULE_SHIP_CANNOT_MOVE:
        return "This Ship Cannot Move";
    case RULE_SHIP_DAMAGED:
        return "Damaged Ship Cannot Move";
    case RULE_NO_MOVES_LEFT:
        return "No Moves Left For This Ship";
    case RULE_CANNOT_TURN_AROUND:
        return "Ship Cannot Turn 180 Degrees";
    case RULE_SHIP_CANNOT_SHOOT:
        return "This Ship Cannot Shoot";
    case RULE_OUT_OF_RANGE:
        return "Target Is Out Of Range";
    case RULE_NO_SHOTS_LEFT:
        return "No Shots Left This Turn";
    case RULE_FRIENDLY_FIRE:
        return "Do Not Shoot Your Own Team";
    case RULE_SHIP_SUNK:
        return "This Ship Is Sunk";
    case RULE_NOT_YOUR_TURN:
        return "It Is Not Your Turn";
    case RULE_NO_SUCH_SHIP:
        return "You Do Not Have This Ship";
    default:
        return "Unknown Rule";
    }
}
