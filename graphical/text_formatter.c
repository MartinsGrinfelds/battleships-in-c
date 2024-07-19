#include "text_formatter.h"
#include <stdio.h> // printf()

void print_failure(char * err_msg)
{
    printf("\033[91m%s\033[0m", err_msg);
}