#pragma once
#include <stdint.h>

#define CONFIG_MAX_PLAYERS 20 // Hard limit so that STATE packet fits all objects (object count is 1 byte).

/// @brief Server settings. Can be set from config file and command line (command line wins).
struct ServerConfig
{
    char bind_address[64]; // Empty means all interfaces.
    int port;
    int max_players; // Max connected clients (players and observers).
    int min_players; // Players needed to close lobby (LOGC-0006).
    int team_count; // 2..4
    int map_width; // 40..255
    int map_height; // 40..255
    int lobby_wait_seconds; // LOGC-0007
    int placement_seconds; // Time to place ships. Then remaining ships are placed randomly.
    int turn_seconds; // Time for one turn. 0 - unlimited.
    int max_turns; // LOGC-0018
    int shots_per_turn;
    int post_game_seconds; // How long to show results before new lobby opens.
    int reconnect_seconds; // How long team without connected players can be away before it loses.
    int verbose;
};

/// @brief Fills config with default values.
void config_defaults(struct ServerConfig *config);

/// @brief Reads "key = value" lines. Lines starting with # are comments.
/// @return 0 on success, -1 on error (error is printed).
int config_load_file(struct ServerConfig *config, const char *path);

/// @brief Parses command line. Handles --config, --help and --version.
/// @return 0 to continue, 1 if program should exit successfully (help/version), -1 on error.
int config_parse_args(struct ServerConfig *config, int argc, char **argv, const char *version);

/// @brief Checks that values make sense.
/// @return 0 if valid, -1 if not (error is printed).
int config_validate(struct ServerConfig *config);

/// @brief Prints config to log.
void config_print(struct ServerConfig *config);
