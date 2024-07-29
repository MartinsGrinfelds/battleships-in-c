#pragma once
#include <stdint.h> // uint definitions

/// @brief Gets user input. Pointer must be freed after.
/// @param min Minimum user input length.
/// @param min Maximum user input length.
/// @return Pointer to input string (input length + \0).
char *get_user_input(uint16_t min, uint16_t max);