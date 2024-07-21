#include "text_formatter.h"
#include <stdio.h> // printf()

void print_failure(char * msg)
{
    printf("\033[91m%s\033[0m", msg);
}

void print_success(char * msg)
{
    printf("\033[92m%s\033[0m", msg);
}