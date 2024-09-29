#include "map.h"
#include <stdio.h>


void update_map_with_objects(struct StatePacket *state, uint8_t *map, uint8_t allied_team)
{
    printf("There are %d objects.\n", state->object_count);
    size_t object_nr = 0;
    while (object_nr < state->object_count)
    {
        object_nr++;
    }
    
    return;
}
