#pragma once
#include <stdint.h> // uint definitions
#include "shared_packets.h" // packets used by battleships

// Need to check if content is not too large if so then there is a problem
#define MAX_CONTENT_SIZE 16384

/// @brief Creates a TCP socket. Can be used by server and client. Prints out any errors if there are some.
/// @return Socket number (negative = errors).
int create_socket();

/// @brief Binds a socket to an address. Must be called after socket creation.
/// @param socket File descriptor for socket.
/// @param port Port to use.
/// @return If negative then error.
int bind_socket_to_address(int socket, uint16_t port);

/// @brief Start to listen to socket.
/// @param socket File descriptor for socket.
/// @param max_requests Maximum queue or connections to socket.
/// @return Listen response.
int listen_to_socket(int socket, int max_requests);

/// @brief Accept connection provided socket.
/// @param socket File descriptor for socket.
/// @return Connected socket/file_descriptor.
int accept_connection(int socket);

/// @brief Receives generic packetr from given socket/file_descriptor.
/// @param socket File descriptor for socket.
/// @return Pointer to GenericPacket created by malloc.
struct GenericPacket* receive_generic_packet(int socket);

/// @brief Closes socket.
/// @param socket File descriptor to close.
void close_socket(int socket);
