#pragma once
#include <stdint.h> // uint definitions
#include <stddef.h> // size_t
#include "shared_packets.h" // packets used by battleships

// Need to check if content is not too large if so then there is a problem
#define MAX_CONTENT_SIZE 100000
// Header on network: sequence number (4), content size (4), packet type (1), checksum (1).
#define PACKET_HEADER_SIZE 10
// If peer does not read data and this much is waiting to be sent then peer is considered broken.
#define MAX_PENDING_SEND (1024 * 1024)

// Connection errors.
#define CONNECTION_OK 0
#define CONNECTION_CLOSED -1 // Peer closed connection or socket failed.
#define CONNECTION_PROTOCOL_ERROR -2 // Peer sent data that can't be parsed. Stream can't be trusted anymore.

/// @brief Wraps a TCP socket with buffers so that packets can be received and sent
/// in pieces without blocking. TCP is a stream: one recv() can return half of a packet
/// or several packets at once. Buffers take care of that.
struct Connection
{
    int socket;
    uint32_t send_sequence; // Next sequence number to send.
    uint32_t last_received_sequence;
    int received_any; // Was any packet received (for sequence check).
    char *receive_buffer;
    size_t receive_length;
    size_t receive_capacity;
    char *send_buffer;
    size_t send_length;
    size_t send_capacity;
    int state; // CONNECTION_OK or error.
    uint32_t dropped_packets; // Packets with wrong checksum or sequence number.
    size_t send_chunk_limit; // Testing only: if not 0, flush sends at most this many bytes per call.
};

/// @brief Creates a TCP socket. Can be used by server and client. Prints out any errors if there are some.
/// @return Socket number (negative = errors).
int create_socket();

/// @brief Binds a socket to an address. Must be called after socket creation.
/// @param socket File descriptor for socket.
/// @param address IP address to bind to (NULL or empty means all interfaces).
/// @param port Port to use.
/// @return If negative then error.
int bind_socket_to_address(int socket, const char *address, uint16_t port);

/// @brief Start to listen to socket.
/// @param socket File descriptor for socket.
/// @param max_requests Maximum queue or connections to socket.
/// @return Listen response.
int listen_to_socket(int socket, int max_requests);

/// @brief Makes given socket non-blocking for possibility to do other tasks before checking socket again.
/// @brief To check if would block check errno (errno == EWOULDBLOCK || errno == EAGAIN) if recv or socket command returns < 0.
/// @brief Else there is problem with socket.
/// @param socket File descriptor for socket.
/// @return 0 on success.
int set_socket_non_blocking(int socket);

/// @brief Connects to host (IP or name) and port. Gives up after timeout.
/// @param host Host name or IP address.
/// @param port Port of server.
/// @param timeout_ms How long to wait for connection.
/// @return Connected non-blocking socket or -1 on failure.
int connect_to_host(const char *host, uint16_t port, int timeout_ms);

/// @brief Accept connection provided socket.
/// @param socket File descriptor for socket.
/// @return Connected socket/file_descriptor. -2 if nothing to accept, -1 on error.
int accept_connection(int socket);

/// @brief Closes socket.
/// @param socket File descriptor to close.
void close_socket(int socket);

/// @brief Prepares connection structure for connected socket.
void connection_init(struct Connection *connection, int socket);

/// @brief Closes socket and frees buffers.
void connection_close(struct Connection *connection);

/// @brief Reads everything available from socket without blocking.
/// @return CONNECTION_OK or error code.
int connection_read(struct Connection *connection);

/// @brief Takes next complete packet from received data.
/// Packets with wrong checksum or sequence number are dropped (counted in dropped_packets).
/// @return Packet created by malloc (free content and packet after use) or NULL if no complete packet yet.
struct GenericPacket *connection_next_packet(struct Connection *connection);

/// @brief Queues packet for sending and tries to send it immediately. Sequence number and checksum are filled in.
/// @return CONNECTION_OK or error code.
int connection_send(struct Connection *connection, uint8_t packet_type, const char *content, size_t content_size);

/// @brief Tries to send queued data without blocking.
/// @return CONNECTION_OK or error code.
int connection_flush(struct Connection *connection);

/// @brief Is there data waiting to be sent.
int connection_has_pending_send(struct Connection *connection);

/// @brief Frees generic packet together with its content.
void free_generic_packet(struct GenericPacket *packet);

/// @brief Calculates checksum by XORing header (without checksum) and content bytes.
uint8_t calculate_checksum(uint32_t sequence_number, uint32_t content_size, uint8_t packet_type, const char *content);
