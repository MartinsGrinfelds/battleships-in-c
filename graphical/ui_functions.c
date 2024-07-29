#include "ui_functions.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h> //stdin

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
