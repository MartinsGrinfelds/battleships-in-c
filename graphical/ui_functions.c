#include "ui_functions.h"
#include "../graphical/text_formatter.h" // print_failure()/succ and warn
#include "map.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h> //stdin
#include <time.h> // Just for fun
#include "../external_libraries/raylib-5.0_linux_amd64/include/raylib.h" // Raylib functions
// #include "../external_libraries/raylib-5.0_linux_amd64/include/raymath.h"
// #include "../external_libraries/raylib-5.0_linux_amd64/include/rlgl.h"

#define MAX_TEXT 5000

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
    int screen_x = GetScreenWidth(), screen_y = GetScreenHeight();
    // printf("Height: %d Width: %d\n", screen_y, screen_x);
    // Calculate screen area used by map.
    screen_x = screen_x * 3 / 4; // Potencial hardcode
    screen_y = screen_y * 3 / 4; // Potencial hardcode
    
    int square_x = screen_x/width, square_y = screen_y/height;
    int x = 0, y = 0;
    Color color = BLANK;
    // srand(time(NULL)); Fog of war potencial radar glitch
    // printf("Height: %f Width: %f\n", screen_y, screen_x);
    // printf("SHeight: %f SWidth: %f\n", square_y, square_x);
    uint8_t object_type;
    
    while (y < height)
    {
        // uint8_t decision = rand() % 5; Fog of war potencial radar glitch
        while (x < width)
        {
            // uint8_t temp_num = map[y*(height)+x]; Fog of war potencial radar glitch (SHOULD BE SERVER SIDE)
            // if (decision == 1 || rand() % 5 == 1)
            // {
            //     map[y*(height)+x] = 666;
            // }
            if (map)
            {
                object_type = map[y*(height)+x];
            }
            else
            {
                // printf("MAP IS NULL!\n");
                object_type = 0;
            }
            // TODO: Add other types
            switch (object_type)
            {
            case 0:
                color = FOW_SEA_COLOR;
                break;
            
            case ALLIED_SHIP:
                color = SHIP_COLOR;
                break;

            case ENEMY_SHIP:
                color = ENEMY_COLOR;
                break;

            default:
                color = GREEN;
            }
            DrawRectangle(x*square_x, y*square_y, square_x, square_y, color);
            DrawRectangleLines(x*square_x, y*square_y, square_x, square_y, SEA_COLOR);
            // map[y*(height)+x] = temp_num; Fog of war potencial radar glitch
            x++;
        }
        x=0;
        y++;
    }
}

char* get_username_input(uint8_t min, uint8_t max, char *message)
{
    // Call free(address) for this one or enjoy memory leak 😍
    char* username = calloc((max + 1), sizeof(char));
    int letter_count = 0, key = 0, font_size = 40, min_text_warning = 0;
    Rectangle text_box = { 240, 180, 500, 50 };
    SetTargetFPS(50);
    

    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        BeginDrawing();

            ClearBackground(RAYWHITE);

            DrawText(message, 240, 140, 20, ENEMY_COLOR);
            int input_size = MeasureText(username, font_size);
            text_box.width = input_size + 10;

            DrawRectangleRec(text_box, LIGHTGRAY);
            DrawRectangleLines((int)text_box.x, (int)text_box.y, (int)text_box.width, (int)text_box.height, ENEMY_COLOR);

            DrawText(username, (int)text_box.x + 5, (int)text_box.y + 8, font_size, SEA_COLOR);

            DrawText(TextFormat("Your input length: %i/%i", letter_count, max), 315, 250, 20, BLACK);

            if(min_text_warning)
            {
                DrawText(TextFormat("Input must be at least: %i characters long!", min), 315, 270, 20, ENEMY_COLOR);
            }

        EndDrawing();

        // Get char pressed (unicode character) on the queue
        key = GetCharPressed();

        // NOTE: Only allow keys in range [32..125]
        if ((key >= 32) && (key <= 125) && (letter_count < max))
        {
            min_text_warning = 0;
            username[letter_count] = (char)key;
            username[letter_count+1] = '\0'; // Add null terminator at the end of the string.
            letter_count++;
        }

        if (IsKeyPressed(KEY_BACKSPACE))
        {
            letter_count--;
            if (letter_count < 0) letter_count = 0;
            username[letter_count] = '\0';
        }
        if (IsKeyPressed(KEY_ENTER))
        {
            if (letter_count < min)
            {
                // TODO: Add audio alert?
                min_text_warning = 1;
            }
            else
            {
                return username;
            }
        }
        key = 0;
    }
    free(username);
    return NULL;
}


/// @brief THIS CODE IS MOST FROM RAYLIB EXAMPLES.
/// @param font 
/// @param text 
/// @param rec 
/// @param fontSize 
/// @param spacing 
/// @param wordWrap 
/// @param tint 
/// @param selectStart 
/// @param selectLength 
/// @param selectTint 
/// @param selectBackTint 
static void DrawTextBoxedSelectable(Font font, const char *text, Rectangle rec, float fontSize, float spacing, bool wordWrap, Color tint, int selectStart, int selectLength, Color selectTint, Color selectBackTint)
{
    int length = TextLength(text);  // Total length in bytes of the text, scanned by codepoints in loop

    float textOffsetY = 0;          // Offset between lines (on line break '\n')
    float textOffsetX = 0.0f;       // Offset X to next character to draw

    float scaleFactor = fontSize/(float)font.baseSize;     // Character rectangle scaling factor

    // Word/character wrapping mechanism variables
    enum { MEASURE_STATE = 0, DRAW_STATE = 1 };
    int state = wordWrap? MEASURE_STATE : DRAW_STATE;

    int startLine = -1;         // Index where to begin drawing (where a line begins)
    int endLine = -1;           // Index where to stop drawing (where a line ends)
    int lastk = -1;             // Holds last value of the character position

    for (int i = 0, k = 0; i < length; i++, k++)
    {
        // Get next codepoint from byte string and glyph index in font
        int codepointByteCount = 0;
        int codepoint = GetCodepoint(&text[i], &codepointByteCount);
        int index = GetGlyphIndex(font, codepoint);

        // NOTE: Normally we exit the decoding sequence as soon as a bad byte is found (and return 0x3f)
        // but we need to draw all of the bad bytes using the '?' symbol moving one byte
        if (codepoint == 0x3f) codepointByteCount = 1;
        i += (codepointByteCount - 1);

        float glyphWidth = 0;
        if (codepoint != '\n')
        {
            glyphWidth = (font.glyphs[index].advanceX == 0) ? font.recs[index].width*scaleFactor : font.glyphs[index].advanceX*scaleFactor;

            if (i + 1 < length) glyphWidth = glyphWidth + spacing;
        }

        // NOTE: When wordWrap is ON we first measure how much of the text we can draw before going outside of the rec container
        // We store this info in startLine and endLine, then we change states, draw the text between those two variables
        // and change states again and again recursively until the end of the text (or until we get outside of the container).
        // When wordWrap is OFF we don't need the measure state so we go to the drawing state immediately
        // and begin drawing on the next line before we can get outside the container.
        if (state == MEASURE_STATE)
        {
            // TODO: There are multiple types of spaces in UNICODE, maybe it's a good idea to add support for more
            // Ref: http://jkorpela.fi/chars/spaces.html
            if ((codepoint == ' ') || (codepoint == '\t') || (codepoint == '\n')) endLine = i;

            if ((textOffsetX + glyphWidth) > rec.width)
            {
                endLine = (endLine < 1)? i : endLine;
                if (i == endLine) endLine -= codepointByteCount;
                if ((startLine + codepointByteCount) == endLine) endLine = (i - codepointByteCount);

                state = !state;
            }
            else if ((i + 1) == length)
            {
                endLine = i;
                state = !state;
            }
            else if (codepoint == '\n') state = !state;

            if (state == DRAW_STATE)
            {
                textOffsetX = 0;
                i = startLine;
                glyphWidth = 0;

                // Save character position when we switch states
                int tmp = lastk;
                lastk = k - 1;
                k = tmp;
            }
        }
        else
        {
            if (codepoint == '\n')
            {
                if (!wordWrap)
                {
                    textOffsetY += (font.baseSize + font.baseSize/2)*scaleFactor;
                    textOffsetX = 0;
                }
            }
            else
            {
                if (!wordWrap && ((textOffsetX + glyphWidth) > rec.width))
                {
                    textOffsetY += (font.baseSize + font.baseSize/2)*scaleFactor;
                    textOffsetX = 0;
                }

                // When text overflows rectangle height limit, just stop drawing
                if ((textOffsetY + font.baseSize*scaleFactor) > rec.height) break;

                // Draw selection background
                bool isGlyphSelected = false;
                if ((selectStart >= 0) && (k >= selectStart) && (k < (selectStart + selectLength)))
                {
                    DrawRectangleRec((Rectangle){ rec.x + textOffsetX - 1, rec.y + textOffsetY, glyphWidth, (float)font.baseSize*scaleFactor }, selectBackTint);
                    isGlyphSelected = true;
                }

                // Draw current character glyph
                if ((codepoint != ' ') && (codepoint != '\t'))
                {
                    DrawTextCodepoint(font, codepoint, (Vector2){ rec.x + textOffsetX, rec.y + textOffsetY }, fontSize, isGlyphSelected? selectTint : tint);
                }
            }

            if (wordWrap && (i == endLine))
            {
                textOffsetY += (font.baseSize + font.baseSize/2)*scaleFactor;
                textOffsetX = 0;
                startLine = endLine;
                endLine = -1;
                glyphWidth = 0;
                selectStart += lastk - k;
                k = lastk;

                state = !state;
            }
        }

        if ((textOffsetX != 0) || (codepoint != ' ')) textOffsetX += glyphWidth;  // avoid leading spaces
    }
}

void show_chat_messages(struct Message* messages)
{
    int screen_x = GetScreenWidth(), screen_y = GetScreenHeight();
    Rectangle container = { 0, (screen_y) * 3 / 4, screen_x * 3 / 4, screen_y / 4};
    // Rectangle resizer = { container.x + container.width - 17, container.y + container.height - 17, 14, 14 };
    Color borderColor = ENEMY_COLOR;
    Font font = GetFontDefault();
    size_t symbols_added = 0, symbols_to_add = 0; // We assume total chatbox character count will not be more than 5000
    char text[MAX_TEXT] = {0};
    bool wordWrap = true;
    struct Message* current_message = messages;
    while (current_message != NULL)
    {
        if (symbols_added > 0)
        {
            // There was messages before. Need to add new line.
            if (symbols_added == MAX_TEXT)
            {
                // Chat buffer full.
                print_failure("CHAT DISPLAY BUFFER IS FULL! REPORT THIS AS BUG! NOTE: YOU WON'T SEE NEW MESSAGES FOR A WHILE!\n");
                break;
            }
            strcat(text, "\n");
            symbols_added++;
            
        }
        
        if (current_message->sender_name == NULL || current_message->receiver_name == NULL || current_message->message == NULL)
        {
            print_failure("CHAT DISPLAY GATHERING FAILURE! REPORT THIS AS BUG!\n");
            if (current_message->next_message != NULL)
            {
                current_message = current_message->next_message;
                continue;
            }
            else
            {
                break;
            }
        }
        // In form: (SENDER)->(RECEIVER):MESSAGE
        symbols_to_add = strlen(current_message->receiver_name) + strlen(current_message->sender_name) + strlen(current_message->message) + 8; // 7 symbols + /0
        if (symbols_to_add + symbols_added > MAX_TEXT)
        {
            print_failure("CHAT DISPLAY BUFFER IS FULL! REPORT THIS AS BUG! NOTE: YOU WON'T SEE NEW MESSAGES FOR A WHILE!\n");
            break;
        }
        strcat(text, "(");
        strcat(text, current_message->sender_name);
        strcat(text, ")->(");
        strcat(text, current_message->receiver_name);
        strcat(text, "):");
        strcat(text, current_message->message);
        symbols_added += symbols_to_add;

        current_message = current_message->next_message;
    }
    

    // BeginDrawing();

    // ClearBackground(RAYWHITE);

    DrawRectangleLinesEx(container, 3, borderColor);    // Draw container border

    // printf("Drawing chat\n");
    // Draw text in container (add some padding)
    DrawTextBoxedSelectable(font, text, (Rectangle){ container.x + 4, container.y + 4, container.width - 4, container.height - 4 }, 20.0f, 2.0f, wordWrap, GRAY, 0, 0, WHITE, WHITE);

    // DrawRectangleRec(resizer, borderColor);             // Draw the resize box

    // Draw bottom info
    // DrawRectangle(0, screen_y - 54, screen_x, 54, GRAY);
    // DrawRectangleRec((Rectangle){ 382.0f, screen_y - 34.0f, 12.0f, 12.0f }, MAROON);

    // DrawText("Word Wrap: ", 313, screen_y-115, 20, BLACK);
    // if (wordWrap) DrawText("ON", 447, screen_y - 115, 20, RED);
    // else DrawText("OFF", 447, screen_y - 115, 20, BLACK);

    // EndDrawing();
}