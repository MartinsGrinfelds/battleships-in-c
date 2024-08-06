#include "ui_functions.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h> //stdin
#include "../external_libraries/raylib-5.0_linux_amd64/include/raylib.h" // Raylib functions
// #include "../external_libraries/raylib-5.0_linux_amd64/include/raymath.h"
// #include "../external_libraries/raylib-5.0_linux_amd64/include/rlgl.h"

char *get_user_input(uint16_t min, uint16_t max)
{
    // FIXME: min and max doesn't work. Replace with graphical library input.
    if (min > max) {
        fprintf(stderr, "Invalid input: min should be less than or equal to max.\n");
        return NULL;
    }
    // Call free(address) for this one or enjoy memory leak 😍
    char *buffer = malloc(max + 1);  // +1 for the null terminator
    if (buffer == NULL) {
        perror("malloc failed");
        return NULL;
    }
    while (1) {
        printf("Please enter a string between %d and %d characters: ", min, max);
        if (fgets(buffer, max + 1, stdin) == NULL) {
            perror("fgets failed");
            free(buffer);
            return NULL;
        }

        // Remove the newline character, if present (not ChatGPT -> and last char \0)
        buffer[strcspn(buffer, "\n")] = '\0';

        // Check the length of the input
        size_t length = strlen(buffer);
        if (length >= min && length <= max) {
            return buffer;  // Valid input, return the buffer
        }

        printf("Input must be between %d and %d characters. You entered %zu characters.\n", min, max, length);
    }
}

void draw_map_area(uint8_t width, uint8_t height, uint8_t *map)
{
    float screen_x = GetScreenWidth(), screen_y = GetScreenHeight();
    // printf("Height: %d Width: %d\n", screen_y, screen_x);
    // Calculate screen area used by map.
    screen_x = screen_x * 3 / 4; // Potencial hardcode
    screen_y = screen_y * 3 / 4; // Potencial hardcode
    float square_x = screen_x/width, square_y = screen_y/height;
    int x = 0, y = 0;
    BeginDrawing();
    // printf("Height: %f Width: %f\n", screen_y, screen_x);
    // printf("SHeight: %f SWidth: %f\n", square_y, square_x);
    while (y < height)
    {
        while (x < width)
        {
            DrawRectangle(x*square_x, y*square_y, square_x, square_y, FOW_SEA_COLOR);
            DrawRectangleLines(x*square_x, y*square_y, square_x, square_y, SEA_COLOR);
            x++;
        }
        x=0;
        y++;
    }
    EndDrawing();
    
}

char* get_username_input(uint8_t max)
{
    int screen_x = GetScreenWidth(), screen_y = GetScreenHeight();
    // Call free(address) for this one or enjoy memory leak 😍
    char* username = calloc((max + 1), sizeof(char));
    int letter_count = 0;
    Rectangle text_box = { screen_x/2.0f - 100, 180, 225, 50 };
    bool activated_text_field = false;
    int frames_counter = 0;
    

    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        if (IsMouseButtonPressed(0))
        {
            // Activate username box if clicked on. Deactivate if clicked away.
            if (CheckCollisionPointRec(GetMousePosition(), text_box) && !activated_text_field)
            {
                activated_text_field = !activated_text_field;
            }
            else if (!CheckCollisionPointRec(GetMousePosition(), text_box) && activated_text_field)
            {
               activated_text_field = false;
            }
            
        }

        if (activated_text_field)
        {
            // Set the window's cursor to the I-Beam
            // SetMouseCursor(MOUSE_CURSOR_IBEAM);

            // Get char pressed (unicode character) on the queue
            int key = GetCharPressed();

            // Check if more characters have been pressed on the same frame
            while (key > 0)
            {
                // NOTE: Only allow keys in range [32..125]
                if ((key >= 32) && (key <= 125) && (letter_count < max))
                {
                    username[letter_count] = (char)key;
                    username[letter_count+1] = '\0'; // Add null terminator at the end of the string.
                    letter_count++;
                }
                key = GetCharPressed();  // Check next character in the queue
            }

            if (IsKeyPressed(KEY_BACKSPACE))
            {
                letter_count--;
                if (letter_count < 0) letter_count = 0;
                username[letter_count] = '\0';
            }
            if (IsKeyPressed(KEY_ENTER))
            {
                return username;
            }
        }
        else
        {
            SetMouseCursor(MOUSE_CURSOR_DEFAULT);
        }

        if (activated_text_field)
        {
            frames_counter++;
        }
        else
        {
            frames_counter = 0;
        }

        BeginDrawing();

            ClearBackground(RAYWHITE);

            DrawText("Enter username!", 240, 140, 20, GRAY);

            DrawRectangleRec(text_box, LIGHTGRAY);
            if (activated_text_field)
            {
                DrawRectangleLines((int)text_box.x, (int)text_box.y, (int)text_box.width, (int)text_box.height, RED);
            }
            else 
            {
                DrawRectangleLines((int)text_box.x, (int)text_box.y, (int)text_box.width, (int)text_box.height, DARKGRAY);
            }

            DrawText(username, (int)text_box.x + 5, (int)text_box.y + 8, 40, MAROON);

            DrawText(TextFormat("INPUT CHARS: %i/%i", letter_count, max), 315, 250, 20, DARKGRAY);

            // if (activated_text_field)
            // {
            //     if (letter_count < max)
            //     {
            //         // Draw blinking underscore char
            //         if (((frames_counter/20)%2) == 0) 
            //         {
            //             DrawText("_", (int)text_box.x + 8 + MeasureText(username, 40), (int)text_box.y + 12, 40, MAROON);
            //         }
            //     }
            //     else
            //     {
            //         DrawText("Press BACKSPACE to delete chars...", 230, 300, 20, GRAY);
            //     }
            // }
        EndDrawing();
    }
    free(username);
    return NULL;
}

// Draw text using font inside rectangle limits
// static void DrawTextBoxed(Font font, const char *text, Rectangle rec, float fontSize, float spacing, bool wordWrap, Color tint)
// {
//     DrawTextBoxedSelectable(font, text, rec, fontSize, spacing, wordWrap, tint, 0, 0, WHITE, WHITE);
// }

void show_chat_messages()
{
    int screen_x = GetScreenWidth(), screen_y = GetScreenHeight();
    Rectangle container = { 0, (screen_y) * 3 / 4, screen_x * 3 / 4, screen_y / 4};
    // Rectangle resizer = { container.x + container.width - 17, container.y + container.height - 17, 14, 14 };
    Color borderColor = ENEMY_COLOR;
    Font font = GetFontDefault();
    char text[] = "Text check... and also... Kā ir latviski? Šausmans";
    // bool wordWrap = true;

    BeginDrawing();

    // ClearBackground(RAYWHITE);

    DrawRectangleLinesEx(container, 3, borderColor);    // Draw container border

    // Draw text in container (add some padding)
    // DrawTextBoxed(font, text, (Rectangle){ container.x + 4, container.y + 4, container.width - 4, container.height - 4 }, 20.0f, 2.0f, wordWrap, GRAY);

    // DrawRectangleRec(resizer, borderColor);             // Draw the resize box

    // Draw bottom info
    // DrawRectangle(0, screen_y - 54, screen_x, 54, GRAY);
    // DrawRectangleRec((Rectangle){ 382.0f, screen_y - 34.0f, 12.0f, 12.0f }, MAROON);

    // DrawText("Word Wrap: ", 313, screen_y-115, 20, BLACK);
    // if (wordWrap) DrawText("ON", 447, screen_y - 115, 20, RED);
    // else DrawText("OFF", 447, screen_y - 115, 20, BLACK);

    EndDrawing();
}