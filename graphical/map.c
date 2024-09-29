#include "map.h"
#include <stdio.h>
#include "../graphical/text_formatter.h" // print_failure()/warning/success
#include <string.h>         // memset()


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
            // This will represent where ship is.
            // TODO: Optimize this!!!
            size_t front_locations[4] = {0};
            int painting_index = 4 - object_type;
            switch (state->map_objects[object_nr].rotation)
            {
            case 0:
                // Rotation UP
                front_locations[0] = map_index - width;
                front_locations[1] = map_index - width*2;
                front_locations[2] = map_index - width*3;
                front_locations[3] = map_index - width*4;
                break;

            case 1:
                // Rotation Right
                front_locations[0] = map_index + 1;
                front_locations[1] = map_index + 2;
                front_locations[2] = map_index + 3;
                front_locations[3] = map_index + 4;
                break;

            case 2:
                // Rotation Down
                front_locations[0] = map_index + width;
                front_locations[1] = map_index + width*2;
                front_locations[2] = map_index + width*3;
                front_locations[3] = map_index + width*4;
                break;

            case 3:
                // Rotation Left
                front_locations[0] = map_index - 1;
                front_locations[1] = map_index - 2;
                front_locations[2] = map_index - 3;
                front_locations[3] = map_index - 4;
                break;
            
            default:
                print_warning("BUG: Wrong rotation for object\n");
                continue;
                // break;
            }
            while (painting_index >= 0)
            {
                map[front_locations[painting_index--]] = ship;
            }
            
        }
        
        object_nr++;
    }
    
    return 0; // TODO: return number of failed objects
}

size_t get_map_index(size_t width, size_t x, size_t y)
{
    return y*width+x;
}
