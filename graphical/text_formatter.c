#include "text_formatter.h"
#include <stdio.h> // printf()
#include <stdarg.h> // va_list
#include <time.h> // time(), localtime_r()
#include <unistd.h> // isatty()

static int minimum_log_level = LOG_LEVEL_INFO;

static int use_colors()
{
    return isatty(STDOUT_FILENO);
}

static void print_colored(const char *color, char *msg)
{
    if (use_colors())
    {
        printf("%s%s\033[0m", color, msg);
    }
    else
    {
        printf("%s", msg);
    }
}

void print_failure(char * msg)
{
    print_colored("\033[91m", msg);
}

void print_warning(char * msg)
{
    print_colored("\033[93m", msg);
}

void print_success(char * msg)
{
    print_colored("\033[92m", msg);
}

void set_log_level(int level)
{
    minimum_log_level = level;
}

void log_message(int level, const char *format, ...)
{
    if (level < minimum_log_level)
    {
        return;
    }
    static const char *level_names[] = {"DEBUG", "INFO", "WARN", "ERROR"};
    static const char *level_colors[] = {"\033[90m", "", "\033[93m", "\033[91m"};
    if (level < LOG_LEVEL_DEBUG || level > LOG_LEVEL_ERROR)
    {
        level = LOG_LEVEL_INFO;
    }
    char time_text[32];
    time_t now = time(NULL);
    struct tm local_time;
    localtime_r(&now, &local_time);
    strftime(time_text, sizeof(time_text), "%Y-%m-%d %H:%M:%S", &local_time);
    int colors = use_colors();
    printf("%s[%s] %-5s ", colors ? level_colors[level] : "", time_text, level_names[level]);
    va_list arguments;
    va_start(arguments, format);
    vprintf(format, arguments);
    va_end(arguments);
    printf("%s\n", colors ? "\033[0m" : "");
    fflush(stdout);
}
