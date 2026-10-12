#include "map.h"
#include <stdio.h>
#include "../graphical/text_formatter.h" // print_failure()/warning/success
#include <string.h>         // memset()
#include "ui_functions.h"
#include "../game/game_logic.h"
#include "../external_libraries/raylib-5.0_linux_amd64/include/raylib.h" // Raylib functions

struct MapLayout get_map_layout(uint8_t width, uint8_t height)
{
    struct MapLayout layout;
    layout.x = 0;
    layout.y = 0;
    layout.width = width ? width : 1;
    layout.height = height ? height : 1;
    int area_x = GetScreenWidth() - STATUS_AREA_WIDTH - 2 * MAP_MARGIN, area_y = GetScreenHeight() - CHAT_AREA_HEIGHT - 2 * MAP_MARGIN;
    int cell_x = area_x / layout.width, cell_y = area_y / layout.height;
    layout.cell = cell_x < cell_y ? cell_x : cell_y;
    if (layout.cell < 2)
    {
        layout.cell = 2;
    }
    // Center map in its area.
    layout.x = MAP_MARGIN + (area_x - layout.cell * layout.width) / 2;
    layout.y = MAP_MARGIN;
    if (layout.x < MAP_MARGIN)
    {
        layout.x = MAP_MARGIN;
    }
    return layout;
}

int add_object(uint8_t *map, uint8_t width, uint8_t height, const struct MapObject *ship, uint8_t ship_id, uint8_t tail_id)
{
    if (!map)
    {
        print_failure("BUG: Map is NULL\n");
        return 1;
    }
    int xs[MAX_SHIP_LENGTH], ys[MAX_SHIP_LENGTH];
    int length = ship_squares(ship, xs, ys);
    if (length == 0)
    {
        return 1;
    }
    int i = 0;
    while (i < length)
    {
        // Never write outside of map (ship can be partly outside while player is placing it).
        if (xs[i] >= 0 && ys[i] >= 0 && xs[i] < width && ys[i] < height)
        {
            map[get_map_index(width, xs[i], ys[i])] = i == 0 ? tail_id : ship_id;
        }
        i++;
    }
    return 0;
}

int update_map_with_objects(struct StatePacket *state, uint8_t *map, uint8_t allied_team, uint8_t player_id)
{
    uint8_t width = state->map_width, height = state->map_height;
    if (!map)
    {
        print_failure("BUG: Map is NULL\n");
        return 1;
    }
    memset(map, FOW_SEA, (size_t)width * height);
    // Draw visible sea first. Fog of war hides everything far from own team ships.
    int y = 0;
    while (y < height)
    {
        int x = 0;
        while (x < width)
        {
            int in_zone = state->status == GAME_STATUS_SHIP_PLACEMENT && allied_team && in_team_zone(allied_team, width, height, x, y);
            if (in_zone)
            {
                map[get_map_index(width, x, y)] = ZONE_SEA;
            }
            else if (state->status == GAME_STATUS_GAME_OVER || state->status == GAME_STATUS_LOBBY ||
                     team_sees_square(allied_team, x, y, state->map_objects, state->object_count))
            {
                map[get_map_index(width, x, y)] = VISIBLE_SEA;
            }
            x++;
        }
        y++;
    }
    size_t object_nr = 0;
    while (object_nr < state->object_count)
    {
        struct MapObject *object = &state->map_objects[object_nr++];
        if (!is_ship(object->object_type))
        {
            continue;
        }
        if (ship_is_sunk(object))
        {
            add_object(map, width, height, object, SUNK_SHIP, SUNK_SHIP);
        }
        else if (object->owner_id == player_id)
        {
            add_object(map, width, height, object, ALLIED_SHIP, ALLIED_SHIP_TAIL);
        }
        else if (allied_team && object->team_id == allied_team)
        {
            add_object(map, width, height, object, TEAMMATE_SHIP, TEAMMATE_SHIP_TAIL);
        }
        else
        {
            add_object(map, width, height, object, ENEMY_SHIP, ENEMY_SHIP_TAIL);
        }
    }
    return 0;
}

size_t get_map_index(size_t width, size_t x, size_t y)
{
    return y*width+x;
}

int get_map_square_from_absolute(uint8_t width, uint8_t height, float absolute_x, float absolute_y, int *x, int *y)
{
    struct MapLayout layout = get_map_layout(width, height);
    float relative_x = absolute_x - layout.x, relative_y = absolute_y - layout.y;
    if (relative_x < 0 || relative_y < 0)
    {
        return -1;
    }
    *x = (int)relative_x / layout.cell;
    *y = (int)relative_y / layout.cell;
    if (*x >= width || *y >= height)
    {
        return -1;
    }
    return 0;
}

static Rectangle square_rectangle(struct MapLayout *layout, int x, int y)
{
    return (Rectangle){(float)(layout->x + x * layout->cell), (float)(layout->y + y * layout->cell), (float)layout->cell, (float)layout->cell};
}

static void draw_cross(Rectangle square, Color color)
{
    float padding = square.width * 0.2f, thickness = square.width > 12 ? 2.5f : 1.5f;
    DrawLineEx((Vector2){square.x + padding, square.y + padding}, (Vector2){square.x + square.width - padding, square.y + square.height - padding}, thickness, color);
    DrawLineEx((Vector2){square.x + square.width - padding, square.y + padding}, (Vector2){square.x + padding, square.y + square.height - padding}, thickness, color);
}

void draw_map_overlays(struct StatePacket *state, uint8_t player_id, uint8_t selected_ship)
{
    struct MapLayout layout = get_map_layout(state->map_width, state->map_height);
    size_t object_nr = 0;
    while (object_nr < state->object_count)
    {
        struct MapObject *object = &state->map_objects[object_nr++];
        if (object->x >= state->map_width || object->y >= state->map_height)
        {
            continue;
        }
        if (object->object_type == OBJECT_MISSED_HIT)
        {
            Rectangle square = square_rectangle(&layout, object->x, object->y);
            DrawCircle((int)(square.x + square.width / 2), (int)(square.y + square.height / 2), square.width * 0.18f, MISS_COLOR);
            continue;
        }
        if (object->object_type == OBJECT_HIT_SQUARE)
        {
            draw_cross(square_rectangle(&layout, object->x, object->y), HIT_COLOR);
            continue;
        }
        if (!is_ship(object->object_type))
        {
            continue;
        }
        int xs[MAX_SHIP_LENGTH], ys[MAX_SHIP_LENGTH];
        int length = ship_squares(object, xs, ys);
        int i = 0;
        while (i < length)
        {
            if (xs[i] >= 0 && ys[i] >= 0 && xs[i] < state->map_width && ys[i] < state->map_height)
            {
                Rectangle square = square_rectangle(&layout, xs[i], ys[i]);
                if (object->info & (1 << i))
                {
                    // Damaged ship segment (wiki: Hit Square shown as X).
                    draw_cross(square, HIT_COLOR);
                }
                else if (i == length - 1 && length > 1)
                {
                    // Small mark on bow so that direction is visible.
                    DrawCircle((int)(square.x + square.width / 2), (int)(square.y + square.height / 2), square.width * 0.15f, BOW_COLOR);
                }
            }
            i++;
        }
        if (selected_ship && object->owner_id == player_id && object->object_type == selected_ship)
        {
            // Outline around selected ship.
            int min_x = xs[0], max_x = xs[0], min_y = ys[0], max_y = ys[0];
            i = 1;
            while (i < length)
            {
                min_x = xs[i] < min_x ? xs[i] : min_x;
                max_x = xs[i] > max_x ? xs[i] : max_x;
                min_y = ys[i] < min_y ? ys[i] : min_y;
                max_y = ys[i] > max_y ? ys[i] : max_y;
                i++;
            }
            Rectangle outline = {(float)(layout.x + min_x * layout.cell - 2), (float)(layout.y + min_y * layout.cell - 2),
                                 (float)((max_x - min_x + 1) * layout.cell + 4), (float)((max_y - min_y + 1) * layout.cell + 4)};
            DrawRectangleLinesEx(outline, 2, SELECTED_COLOR);
        }
    }
}

void draw_square_highlight(uint8_t width, uint8_t height, int x, int y, int thickness, uint32_t color_rgba)
{
    if (x < 0 || y < 0 || x >= width || y >= height)
    {
        return;
    }
    struct MapLayout layout = get_map_layout(width, height);
    Color color = GetColor(color_rgba);
    DrawRectangleLinesEx(square_rectangle(&layout, x, y), (float)thickness, color);
}
