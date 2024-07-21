#include "ui_functions.h"
#include <string.h>
#include <stdlib.h> 

char *get_user_input(uint16_t min, uint16_t max)
{
    // TODO: Implement min max check do not allow different input.
    // Call free(address) for this one or enjoy memory leak 😍
    char *input = malloc(sizeof("Player Nr1 aka Fruit\0"));
    strcpy(input, "Player Nr1 aka Fruit");
    min = 0; // to surpress warning
    max = 30; // to surpress warning
    return input;
}
