#include "connection.h"
#include <sys/socket.h>     // socket(), AF_INET, SOCK_STREAM
#include <netinet/in.h>     // sockaddr_in struct
#include <netinet/tcp.h>    // TCP_NODELAY
#include <netdb.h>          // getaddrinfo()
#include <stdio.h>          // perror()
#include <stdlib.h>         // exit(), size_t, malloc
#include <string.h>         // memset()
#include <unistd.h>         // close()
#include "shared_packets.h" // packets used by battleships
#include <arpa/inet.h>      // inet_pton(), htonl()
#include <fcntl.h>          // F_GETFL, O_NONBLOCK, F_SETFL
#include <errno.h>          // errno, EWOULDBLOCK, EAGAIN
#include <poll.h>           // poll()
#include "../graphical/text_formatter.h" // log_message()

#ifndef MSG_NOSIGNAL
// macOS does not have MSG_NOSIGNAL. SIGPIPE is ignored by programs instead.
#define MSG_NOSIGNAL 0
#endif

int create_socket()
{
    // AF_INET      IPv4 Internet protocols
    int tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_socket < 0)
    {
        perror("Error: ");
        return tcp_socket;
    }
    // Allow server restart without waiting for old connections to time out.
    int enable = 1;
    setsockopt(tcp_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
    return tcp_socket;
}

int bind_socket_to_address(int socket, const char *address, uint16_t port)
{
    struct sockaddr_in socket_address;
    memset(&socket_address, 0, sizeof(socket_address)); // Clean possible garbage from this.
    socket_address.sin_family = AF_INET;
    socket_address.sin_addr.s_addr = htonl(INADDR_ANY);
    socket_address.sin_port = htons(port);
    if (address && address[0] && inet_pton(AF_INET, address, &socket_address.sin_addr) != 1)
    {
        log_message(LOG_LEVEL_ERROR, "Invalid bind address: %s", address);
        return -1;
    }
    int bind_return = bind(socket, (struct sockaddr *)&socket_address, sizeof(socket_address));
    if (bind_return < 0)
    {
        perror("Error: ");
        return bind_return;
    }
    return bind_return;
}

int listen_to_socket(int socket, int max_requests)
{
    int listen_response = listen(socket, max_requests);
    if (listen_response < 0)
    {
        perror("Error: ");
        return listen_response;
    }
    return listen_response;
}

int set_socket_non_blocking(int socket)
{
    int flags = fcntl(socket, F_GETFL, 0);
    if (flags == -1)
    {
        perror("fcntl F_GETFL");
        return 1;
    }

    flags |= O_NONBLOCK;
    if (fcntl(socket, F_SETFL, flags) == -1)
    {
        perror("fcntl F_SETFL");
        return 2;
    }
    return 0;
}

static void set_socket_options(int socket)
{
    int enable = 1;
    // Game packets are small and should be delivered immediately.
    setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(enable));
#ifdef SO_NOSIGPIPE
    setsockopt(socket, SOL_SOCKET, SO_NOSIGPIPE, &enable, sizeof(enable));
#endif
}

int connect_to_host(const char *host, uint16_t port, int timeout_ms)
{
    struct addrinfo hints, *results = NULL, *result;
    char port_text[8];
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(port_text, sizeof(port_text), "%u", port);
    int error = getaddrinfo(host, port_text, &hints, &results);
    if (error != 0)
    {
        log_message(LOG_LEVEL_ERROR, "Can't resolve host %s: %s", host, gai_strerror(error));
        return -1;
    }
    int connected_socket = -1;
    for (result = results; result != NULL && connected_socket < 0; result = result->ai_next)
    {
        int tcp_socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
        if (tcp_socket < 0)
        {
            continue;
        }
        set_socket_non_blocking(tcp_socket);
        int status = connect(tcp_socket, result->ai_addr, result->ai_addrlen);
        if (status < 0 && errno == EINPROGRESS)
        {
            // Wait for connection to finish but not forever.
            struct pollfd poll_info = {.fd = tcp_socket, .events = POLLOUT};
            status = -1;
            if (poll(&poll_info, 1, timeout_ms) == 1)
            {
                int socket_error = 0;
                socklen_t length = sizeof(socket_error);
                getsockopt(tcp_socket, SOL_SOCKET, SO_ERROR, &socket_error, &length);
                status = socket_error == 0 ? 0 : -1;
                errno = socket_error;
            }
            else
            {
                errno = ETIMEDOUT;
            }
        }
        if (status == 0)
        {
            set_socket_options(tcp_socket);
            connected_socket = tcp_socket;
        }
        else
        {
            close(tcp_socket);
        }
    }
    freeaddrinfo(results);
    return connected_socket;
}

int accept_connection(int socket)
{
    struct sockaddr_in client_address;
    memset(&client_address, 0, sizeof(client_address)); // Clean possible garbage from this.
    socklen_t client_socket_len = sizeof(client_address);
    // This can be either blocking or non-blocking based of socket settings.
    int client_socket = accept(socket, (struct sockaddr *)&client_address, &client_socket_len);
    if (client_socket < 0)
    {
        if (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR)
        {
            // No incomming connections now. Continuing...
            return -2;
        }
        perror("Error: ");
        return -1;
    }
    set_socket_options(client_socket);
    char address_text[INET_ADDRSTRLEN] = "?";
    inet_ntop(AF_INET, &client_address.sin_addr, address_text, sizeof(address_text));
    log_message(LOG_LEVEL_INFO, "New connection from %s:%d (socket %d)", address_text, ntohs(client_address.sin_port), client_socket);
    return client_socket;
}

void close_socket(int socket)
{
    if (socket >= 0)
    {
        close(socket);
    }
}

void connection_init(struct Connection *connection, int socket)
{
    memset(connection, 0, sizeof(struct Connection));
    connection->socket = socket;
    connection->send_sequence = 1; // 0 is reserved for sequence number overflow.
    connection->state = socket >= 0 ? CONNECTION_OK : CONNECTION_CLOSED;
}

void connection_close(struct Connection *connection)
{
    close_socket(connection->socket);
    free(connection->receive_buffer);
    free(connection->send_buffer);
    connection_init(connection, -1);
}

/// @brief Makes sure buffer can hold needed amount of bytes.
static int ensure_capacity(char **buffer, size_t *capacity, size_t needed)
{
    if (*capacity >= needed)
    {
        return 0;
    }
    size_t new_capacity = *capacity ? *capacity : 4096;
    while (new_capacity < needed)
    {
        new_capacity *= 2;
    }
    char *new_buffer = realloc(*buffer, new_capacity);
    if (!new_buffer)
    {
        return -1;
    }
    *buffer = new_buffer;
    *capacity = new_capacity;
    return 0;
}

int connection_read(struct Connection *connection)
{
    if (connection->state != CONNECTION_OK)
    {
        return connection->state;
    }
    while (1)
    {
        if (ensure_capacity(&connection->receive_buffer, &connection->receive_capacity, connection->receive_length + 4096))
        {
            connection->state = CONNECTION_CLOSED;
            return connection->state;
        }
        ssize_t received = recv(connection->socket, connection->receive_buffer + connection->receive_length,
                                connection->receive_capacity - connection->receive_length, 0);
        if (received > 0)
        {
            connection->receive_length += received;
            if (connection->receive_length > MAX_CONTENT_SIZE * 4)
            {
                // Peer sends more than it is reasonable to keep. Packets are not being processed.
                connection->state = CONNECTION_PROTOCOL_ERROR;
                return connection->state;
            }
            continue;
        }
        if (received == 0)
        {
            // Peer closed connection.
            connection->state = CONNECTION_CLOSED;
            return connection->state;
        }
        if (errno == EINTR)
        {
            continue;
        }
        if (errno == EWOULDBLOCK || errno == EAGAIN)
        {
            return CONNECTION_OK;
        }
        connection->state = CONNECTION_CLOSED;
        return connection->state;
    }
}

static uint32_t read_u32(const char *data)
{
    uint32_t value;
    memcpy(&value, data, sizeof(value));
    return ntohl(value);
}

static void write_u32(char *data, uint32_t value)
{
    value = htonl(value);
    memcpy(data, &value, sizeof(value));
}

uint8_t calculate_checksum(uint32_t sequence_number, uint32_t content_size, uint8_t packet_type, const char *content)
{
    char header[9];
    write_u32(header, sequence_number);
    write_u32(header + 4, content_size);
    header[8] = (char)packet_type;
    uint8_t checksum = 0;
    size_t i = 0;
    while (i < sizeof(header))
    {
        checksum ^= (uint8_t)header[i++];
    }
    i = 0;
    while (i < content_size)
    {
        checksum ^= (uint8_t)content[i++];
    }
    return checksum;
}

/// @brief Wiki: packet is invalid if sequence number is lower than previously received (except if it is 0).
static int sequence_is_valid(struct Connection *connection, uint32_t sequence_number)
{
    if (!connection->received_any || sequence_number == 0)
    {
        return 1;
    }
    return sequence_number > connection->last_received_sequence;
}

struct GenericPacket *connection_next_packet(struct Connection *connection)
{
    while (connection->state == CONNECTION_OK || connection->receive_length > 0)
    {
        if (connection->receive_length < PACKET_HEADER_SIZE)
        {
            return NULL;
        }
        char *data = connection->receive_buffer;
        uint32_t sequence_number = read_u32(data);
        uint32_t content_size = read_u32(data + 4);
        uint8_t packet_type = (uint8_t)data[8];
        uint8_t checksum = (uint8_t)data[9];
        if (content_size > MAX_CONTENT_SIZE)
        {
            // Can't know where next packet starts. Stream is broken.
            log_message(LOG_LEVEL_WARNING, "Packet content size %u is too big (socket %d)", content_size, connection->socket);
            connection->state = CONNECTION_PROTOCOL_ERROR;
            connection->receive_length = 0;
            return NULL;
        }
        if (connection->receive_length < PACKET_HEADER_SIZE + content_size)
        {
            // Rest of packet hasn't arrived yet.
            return NULL;
        }
        char *content = data + PACKET_HEADER_SIZE;
        int valid = 1;
        if (calculate_checksum(sequence_number, content_size, packet_type, content) != checksum)
        {
            log_message(LOG_LEVEL_WARNING, "Dropping packet with wrong checksum (socket %d)", connection->socket);
            valid = 0;
        }
        else if (!sequence_is_valid(connection, sequence_number))
        {
            log_message(LOG_LEVEL_WARNING, "Dropping packet with old sequence number %u (socket %d)", sequence_number, connection->socket);
            valid = 0;
        }
        struct GenericPacket *packet = NULL;
        if (valid)
        {
            connection->received_any = 1;
            connection->last_received_sequence = sequence_number;
            // Call free(address) for this one or enjoy memory leak 😍
            packet = malloc(sizeof(struct GenericPacket));
            packet->sequence_number = sequence_number;
            packet->packet_content_size = content_size;
            packet->packet_type = packet_type;
            packet->checksum = checksum;
            packet->content = malloc(content_size + 1);
            memcpy(packet->content, content, content_size);
            packet->content[content_size] = '\0';
        }
        else
        {
            connection->dropped_packets++;
        }
        // Remove processed packet from buffer.
        size_t processed = PACKET_HEADER_SIZE + content_size;
        memmove(connection->receive_buffer, connection->receive_buffer + processed, connection->receive_length - processed);
        connection->receive_length -= processed;
        if (packet)
        {
            return packet;
        }
    }
    return NULL;
}

int connection_flush(struct Connection *connection)
{
    if (connection->state != CONNECTION_OK)
    {
        return connection->state;
    }
    size_t sent_total = 0;
    while (sent_total < connection->send_length)
    {
        size_t to_send = connection->send_length - sent_total;
        if (connection->send_chunk_limit && to_send > connection->send_chunk_limit)
        {
            to_send = connection->send_chunk_limit;
        }
        ssize_t sent = send(connection->socket, connection->send_buffer + sent_total, to_send, MSG_NOSIGNAL);
        if (sent > 0)
        {
            sent_total += sent;
            if (connection->send_chunk_limit)
            {
                // Testing: packets arrive to peer in small pieces.
                break;
            }
            continue;
        }
        if (sent < 0 && errno == EINTR)
        {
            continue;
        }
        if (sent < 0 && (errno == EWOULDBLOCK || errno == EAGAIN))
        {
            // Socket buffer is full. Rest will be sent later.
            break;
        }
        connection->state = CONNECTION_CLOSED;
        return connection->state;
    }
    memmove(connection->send_buffer, connection->send_buffer + sent_total, connection->send_length - sent_total);
    connection->send_length -= sent_total;
    return CONNECTION_OK;
}

int connection_send(struct Connection *connection, uint8_t packet_type, const char *content, size_t content_size)
{
    if (connection->state != CONNECTION_OK)
    {
        return connection->state;
    }
    if (content_size > MAX_CONTENT_SIZE)
    {
        log_message(LOG_LEVEL_ERROR, "BUG: Trying to send too big packet (%zu bytes)", content_size);
        return CONNECTION_OK;
    }
    if (connection->send_length + PACKET_HEADER_SIZE + content_size > MAX_PENDING_SEND)
    {
        // Peer doesn't read data. It is better to disconnect than to use all memory.
        log_message(LOG_LEVEL_WARNING, "Peer is not reading data (socket %d)", connection->socket);
        connection->state = CONNECTION_CLOSED;
        return connection->state;
    }
    if (ensure_capacity(&connection->send_buffer, &connection->send_capacity, connection->send_length + PACKET_HEADER_SIZE + content_size))
    {
        connection->state = CONNECTION_CLOSED;
        return connection->state;
    }
    uint32_t sequence_number = connection->send_sequence;
    // If the sequence number reaches the maximum value it starts again from 0.
    connection->send_sequence = (sequence_number == UINT32_MAX) ? 0 : sequence_number + 1;
    char *header = connection->send_buffer + connection->send_length;
    write_u32(header, sequence_number);
    write_u32(header + 4, (uint32_t)content_size);
    header[8] = (char)packet_type;
    header[9] = (char)calculate_checksum(sequence_number, (uint32_t)content_size, packet_type, content);
    if (content_size)
    {
        memcpy(header + PACKET_HEADER_SIZE, content, content_size);
    }
    connection->send_length += PACKET_HEADER_SIZE + content_size;
    return connection_flush(connection);
}

int connection_has_pending_send(struct Connection *connection)
{
    return connection->send_length > 0;
}

void free_generic_packet(struct GenericPacket *packet)
{
    if (packet)
    {
        free(packet->content);
        free(packet);
    }
}
