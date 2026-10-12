#pragma once

// Log levels for log_message().
#define LOG_LEVEL_DEBUG 0
#define LOG_LEVEL_INFO 1
#define LOG_LEVEL_WARNING 2
#define LOG_LEVEL_ERROR 3

/// @brief Prints a message to stdout with red color.
/// @param msg Message string
void print_failure(char * msg);

/// @brief Prints a message to stdout with yellow color.
/// @param msg Message string
void print_warning(char * msg);

/// @brief Prints a message to stdout with green color.
/// @param msg Message string
void print_success(char * msg);

/// @brief Prints a log line with time and level. Colors are used only when output is a terminal.
/// @param level LOG_LEVEL_DEBUG, LOG_LEVEL_INFO, LOG_LEVEL_WARNING or LOG_LEVEL_ERROR.
/// @param format printf() style format.
void log_message(int level, const char *format, ...) __attribute__((format(printf, 2, 3)));

/// @brief Messages below this level are not printed. Default LOG_LEVEL_INFO.
void set_log_level(int level);
