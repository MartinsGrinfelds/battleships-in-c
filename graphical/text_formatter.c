#include "text_formatter.h"
#include <stdio.h> // printf()

// TODO: Reformat in form [TYPE]: something
void print_failure(char * msg)
{
    printf("\033[91m%s\033[0m", msg);
}

void print_warning(char * msg)
{
    printf("\033[93m%s\033[0m", msg);
}

void print_success(char * msg)
{
    printf("\033[92m%s\033[0m", msg);
}