#pragma once
#include <stdint.h> // uint definitions

/// @brief Creates a TCP socket. Can be used by server and client. Prints out any errors if there are some.
/// @return Socket number (negative = errors).
int create_socket();

/// @brief Binds a socket to an address. Must be called after socket creation.
/// @param socket File descriptor for socket.
/// @param port Port to use.
/// @return If negative then error.
int bind_socket_to_address(int socket, uint16_t port);

/// @brief Closes socket.
/// @param socket File descriptor to close.
void close_socket(int socket);