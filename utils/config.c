#include "config.h"
#include "../graphical/text_formatter.h" // log_message()
#include <stdio.h>
#include <stdlib.h> // strtol()
#include <string.h>
#include <ctype.h> // isspace()

/// @brief One configurable setting. Same name is used in file and as --name on command line.
struct Setting
{
    const char *name;
    const char *description;
    int *value; // For integer settings.
    int minimum;
    int maximum;
};

static int fill_settings(struct ServerConfig *config, struct Setting *settings)
{
    struct Setting all[] = {
        {"port", "TCP port to listen on", &config->port, 1, 65535},
        {"max-players", "Max connected players and observers", &config->max_players, 2, CONFIG_MAX_PLAYERS},
        {"min-players", "Players needed to start a game", &config->min_players, 2, CONFIG_MAX_PLAYERS},
        {"teams", "Team count", &config->team_count, 2, 4},
        {"map-width", "Map width in squares", &config->map_width, 40, 255},
        {"map-height", "Map height in squares", &config->map_height, 40, 255},
        {"lobby-wait", "Seconds to wait for more players (new player resets timer)", &config->lobby_wait_seconds, 0, 3600},
        {"placement-time", "Seconds to place ships (then they are placed randomly)", &config->placement_seconds, 5, 3600},
        {"turn-time", "Seconds for one turn (0 - unlimited)", &config->turn_seconds, 0, 255},
        {"max-turns", "Game ends after this many turns", &config->max_turns, 1, 65535},
        {"shots-per-turn", "Shots player can fire each turn", &config->shots_per_turn, 1, 10},
        {"post-game", "Seconds to show results before new lobby", &config->post_game_seconds, 0, 3600},
        {"reconnect-time", "Seconds a team can be disconnected before it loses", &config->reconnect_seconds, 0, 3600},
        {"verbose", "1 - print debug messages", &config->verbose, 0, 1},
    };
    size_t count = sizeof(all) / sizeof(all[0]);
    memcpy(settings, all, sizeof(all));
    return (int)count;
}

#define MAX_SETTINGS 20

void config_defaults(struct ServerConfig *config)
{
    memset(config, 0, sizeof(struct ServerConfig));
    config->port = 12345;
    config->max_players = 10;
    config->min_players = 2;
    config->team_count = 2;
    config->map_width = 50;
    config->map_height = 50;
    config->lobby_wait_seconds = 30;
    config->placement_seconds = 120;
    config->turn_seconds = 60;
    config->max_turns = 150;
    config->shots_per_turn = 1;
    config->post_game_seconds = 15;
    config->reconnect_seconds = 60;
    config->verbose = 0;
}

static char *trim(char *text)
{
    while (isspace((unsigned char)*text))
    {
        text++;
    }
    char *end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1]))
    {
        *--end = '\0';
    }
    return text;
}

/// @brief Sets one setting from text.
/// @return 0 on success, -1 on error.
static int set_value(struct ServerConfig *config, const char *name, const char *value, const char *source)
{
    if (strcmp(name, "bind") == 0)
    {
        if (strlen(value) >= sizeof(config->bind_address))
        {
            log_message(LOG_LEVEL_ERROR, "%s: bind address is too long", source);
            return -1;
        }
        strcpy(config->bind_address, value);
        return 0;
    }
    struct Setting settings[MAX_SETTINGS];
    int count = fill_settings(config, settings);
    int i = 0;
    while (i < count)
    {
        if (strcmp(settings[i].name, name) == 0)
        {
            char *end = NULL;
            long number = strtol(value, &end, 10);
            if (!value[0] || *end != '\0')
            {
                log_message(LOG_LEVEL_ERROR, "%s: value of %s must be a number (got \"%s\")", source, name, value);
                return -1;
            }
            if (number < settings[i].minimum || number > settings[i].maximum)
            {
                log_message(LOG_LEVEL_ERROR, "%s: %s must be from %d to %d (got %ld)", source, name, settings[i].minimum, settings[i].maximum, number);
                return -1;
            }
            *settings[i].value = (int)number;
            return 0;
        }
        i++;
    }
    log_message(LOG_LEVEL_ERROR, "%s: unknown setting \"%s\" (see --help)", source, name);
    return -1;
}

int config_load_file(struct ServerConfig *config, const char *path)
{
    FILE *file = fopen(path, "r");
    if (!file)
    {
        log_message(LOG_LEVEL_ERROR, "Can't open config file %s", path);
        return -1;
    }
    char line[512], source[600];
    int line_number = 0, result = 0;
    while (fgets(line, sizeof(line), file))
    {
        line_number++;
        char *text = trim(line);
        if (text[0] == '\0' || text[0] == '#')
        {
            continue;
        }
        snprintf(source, sizeof(source), "%s:%d", path, line_number);
        char *separator = strchr(text, '=');
        if (!separator)
        {
            log_message(LOG_LEVEL_ERROR, "%s: expected \"key = value\"", source);
            result = -1;
            continue;
        }
        *separator = '\0';
        if (set_value(config, trim(text), trim(separator + 1), source))
        {
            result = -1;
        }
    }
    fclose(file);
    return result;
}

static void print_help(struct ServerConfig *config, const char *program)
{
    struct Setting settings[MAX_SETTINGS];
    int count = fill_settings(config, settings);
    printf("Usage: %s [options]\n\n", program);
    printf("Options (same names can be used as \"name = value\" in config file):\n");
    printf("  --config FILE           Read settings from file (command line options win)\n");
    printf("  --bind ADDRESS          IPv4 address to listen on (default: all)\n");
    int i = 0;
    while (i < count)
    {
        char option[64];
        snprintf(option, sizeof(option), "--%s N", settings[i].name);
        printf("  %-23s %s (default: %d)\n", option, settings[i].description, *settings[i].value);
        i++;
    }
    printf("  --help                  Show this help\n");
    printf("  --version               Show version\n");
}

int config_parse_args(struct ServerConfig *config, int argc, char **argv, const char *version)
{
    // Config file is read first so that command line can override it.
    int i = 1;
    while (i < argc)
    {
        if (strcmp(argv[i], "--config") == 0)
        {
            if (i + 1 >= argc)
            {
                log_message(LOG_LEVEL_ERROR, "--config needs a file name");
                return -1;
            }
            if (config_load_file(config, argv[i + 1]))
            {
                return -1;
            }
        }
        i++;
    }
    i = 1;
    while (i < argc)
    {
        const char *argument = argv[i];
        if (strcmp(argument, "--help") == 0 || strcmp(argument, "-h") == 0)
        {
            print_help(config, argv[0]);
            return 1;
        }
        if (strcmp(argument, "--version") == 0)
        {
            printf("%s\n", version);
            return 1;
        }
        if (strncmp(argument, "--", 2) != 0)
        {
            log_message(LOG_LEVEL_ERROR, "Unexpected argument \"%s\" (see --help)", argument);
            return -1;
        }
        if (i + 1 >= argc)
        {
            log_message(LOG_LEVEL_ERROR, "%s needs a value", argument);
            return -1;
        }
        if (strcmp(argument, "--config") != 0 && set_value(config, argument + 2, argv[i + 1], "command line"))
        {
            return -1;
        }
        i += 2;
    }
    return 0;
}

int config_validate(struct ServerConfig *config)
{
    if (config->min_players > config->max_players)
    {
        log_message(LOG_LEVEL_ERROR, "min-players (%d) can't be bigger than max-players (%d)", config->min_players, config->max_players);
        return -1;
    }
    return 0;
}

void config_print(struct ServerConfig *config)
{
    log_message(LOG_LEVEL_INFO, "Listening on %s:%d", config->bind_address[0] ? config->bind_address : "0.0.0.0", config->port);
    log_message(LOG_LEVEL_INFO, "Players %d..%d, teams %d, map %dx%d", config->min_players, config->max_players, config->team_count, config->map_width, config->map_height);
    log_message(LOG_LEVEL_INFO, "Lobby wait %ds, placement %ds, turn %ds, max turns %d, shots per turn %d",
                config->lobby_wait_seconds, config->placement_seconds, config->turn_seconds, config->max_turns, config->shots_per_turn);
}
