#include "map.h"
#include <stdio.h>
#include "../graphical/text_formatter.h" // print_failure()/warning/success
#include <string.h>         // memset()
#include "ui_functions.h"
#include "../external_libraries/raylib-5.0_linux_amd64/include/raylib.h" // Raylib functions

int add_object(uint8_t *map, uint8_t width, size_t index, uint8_t rotation, uint8_t object_type, uint8_t ship_id)
{
    // This will represent where ship is.
    // TODO: Optimize this!!!
    size_t front_locations[4] = {0};
    int painting_index = 4 - object_type;
    switch (rotation)
    {
    case 0:
        // Rotation UP
        front_locations[0] = index;
        front_locations[1] = index - width;
        front_locations[2] = index - width*2;
        front_locations[3] = index - width*3;
        break;

    case 1:
        // Rotation Right
        front_locations[0] = index;
        front_locations[1] = index + 1;
        front_locations[2] = index + 2;
        front_locations[3] = index + 3;
        break;

    case 2:
        // Rotation Down
        front_locations[0] = index;
        front_locations[1] = index + width;
        front_locations[2] = index + width*2;
        front_locations[3] = index + width*3;
        break;

    case 3:
        // Rotation Left
        front_locations[0] = index;
        front_locations[1] = index - 1;
        front_locations[2] = index - 2;
        front_locations[3] = index - 3;
        break;
    
    default:
        print_failure("BUG: Wrong rotation for object\n");
        return 1;
    }
    while (painting_index >= 0)
    {
        map[front_locations[painting_index--]] = ship_id;
    }
    return 0;
}

int update_map_with_objects(struct StatePacket *state, uint8_t *map, uint8_t allied_team)
{
    uint8_t width = state->map_width, height = state->map_height;
    memset(map, 0, width * height);
    if (!map)
    {
        print_failure("BUG: Map is NULL\n");
    }
    size_t object_nr = 0;
    while (object_nr < state->object_count)
    {
        uint8_t x = state->map_objects[object_nr].x, y = state->map_objects[object_nr].y;
        if (x >= width || y >= height)
        {
            print_warning("BUG: Map object out of map!\n");
            printf("BUG INFO: Object type(%d), X(%d), Y(%d)\n", state->map_objects[object_nr].object_type, x, y);
            continue;
        }
        size_t map_index = get_map_index(width, x, y);
        uint8_t object_type = state->map_objects[object_nr].object_type;
        if (object_type > 0 && object_type < 6)
        {
            uint8_t ship = allied_team == state->map_objects[object_nr].team_id;
            if (ship)
            {
                map[map_index] = ALLIED_SHIP_TAIL;
                ship = ALLIED_SHIP;
            }
            else
            {
                map[map_index] = ENEMY_SHIP_TAIL;
                ship = ENEMY_SHIP;
            }
            add_object(map, width, map_index, state->map_objects[object_nr].rotation, object_type, ship);
        }
        object_nr++;
    }
    
    return 0; // TODO: return number of failed objects
}

size_t get_map_index(size_t width, size_t x, size_t y)
{
    return y*width+x;
}

int get_map_index_from_absolute(size_t width, float absolute_x, float absolute_y)
{
    size_t relative_x, relative_y;
    int screen_x = GetScreenWidth(), screen_y = GetScreenHeight();
    // Calculate screen area used by map.
    screen_x = screen_x * MAP_AREA_PERCENTAGE;
    screen_y = screen_y * MAP_AREA_PERCENTAGE;
    if (absolute_x < 0 || absolute_y < 0 || absolute_x > screen_x || absolute_y > screen_y)
    {
        return -1;
    }
    relative_x = (absolute_x/(screen_x)) * width;
    relative_y = (absolute_y/(screen_y)) * width;
    return get_map_index(width, relative_x, relative_y);
}
