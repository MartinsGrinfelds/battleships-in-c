#define _POSIX_C_SOURCE 200809L   // request POSIX.1-2008 + POSIX.1-2001 extensions. For clock_gettime()

#include "connection.h"
#include <sys/socket.h>     // socket(), AF_INET, SOCK_STREAM
#include <netinet/ip.h>     // sockaddr_in struct
#include <stdio.h>          // perror()
#include <stdlib.h>         // exit(), size_t, malloc
#include <string.h>         // memset()
#include <unistd.h>         // close()
#include "shared_packets.h" // packets used by battleships
#include <arpa/inet.h>      // inet_pton()
#include <fcntl.h>          // F_GETFL, O_NONBLOCK, F_SETFL
#include <errno.h>          // errno, EWOULDBLOCK, EAGAIN
#include <time.h>           // clock_gettime(), timespec, CLOCK_MONOTONIC
#include "../graphical/text_formatter.h" // print_failure(), print_success(), print_warning()

int create_socket()
{
    // AF_INET      IPv4 Internet protocols
    int tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_socket < 0)
    {
        perror("Error: ");
        return tcp_socket;
    }
    printf("Socket with ID %d created!\n", tcp_socket);
    return tcp_socket;
}

int bind_socket_to_address(int socket, uint16_t port)
{
    struct sockaddr_in socket_address;
    memset(&socket_address, 0, sizeof(socket_address)); // Clean possible garbage from this.
    socket_address.sin_family = AF_INET;
    socket_address.sin_addr.s_addr = INADDR_ANY;
    socket_address.sin_port = htons(port);
    int bint_return = bind(socket, (struct sockaddr *)&socket_address, sizeof(socket_address));

    if (bint_return < 0)
    {
        perror("Error: ");
        return bint_return;
    }
    printf("(Socket binded!) Binding response: %d For socket: %d\n", bint_return, socket);
    return bint_return;
}

int listen_to_socket(int socket, int max_requests)
{
    int listen_response = listen(socket, max_requests);
    if (listen_response < 0)
    {
        perror("Error: ");
        return listen_response;
    }
    printf("(Listening!) Listen response: %d For socket: %d\n", listen_response, socket);
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

int connect_socket(int socket, uint16_t port, char *ip)
{
    struct sockaddr_in socket_address;
    memset(&socket_address, 0, sizeof(socket_address)); // Clean possible garbage from this.
    socket_address.sin_family = AF_INET;
    socket_address.sin_port = htons(port);
    inet_pton(AF_INET, ip, &socket_address.sin_addr);
    if (connect(socket, (struct sockaddr *)&socket_address, sizeof(socket_address)) < 0)
    {
        perror("Error: ");
        return -1;
    }
    return 0;
}

int accept_connection(int socket)
{
    struct sockaddr_in client_address;
    memset(&client_address, 0, sizeof(client_address)); // Clean possible garbage from this.
    socklen_t client_socket_len = sizeof(client_address);
    // This can be either blocking or non-blocking based of socket settings.
    // printf("Waiting for a new connection for socket: %d\n", socket);
    int client_socket = accept(socket, (struct sockaddr *)&client_address, &client_socket_len);
    if (client_socket < 0)
    {
        if (errno == EWOULDBLOCK || errno == EAGAIN)
        {
            // printf("No incomming connections now. Continuing...\n");
            return -2;
        }
        else
        {
            perror("Error: ");
            return -1;
        }
    }
    return client_socket;
}

// Returns current time in milliseconds. Based on MONOTONIC clock (when system was started).
static inline int64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// Try to receive exact data with some timeout in milliseconds or fail.
// Returns:
//  0 - success
// -1 - would block on first byte and do_not_wait_first_byte is set
// -2 - timeout
// -3 - peer closed connection during read
// -4 - other recv error (check errno)
int recv_exact_or_timeout(int fd, void *buf, size_t len, int timeout_ms, uint8_t do_not_wait_first_byte) {
    uint8_t *p = (uint8_t*)buf;
    size_t got = 0;
    int64_t start = now_ms();

    while (got < len) {
        ssize_t n = recv(fd, p + got, len - got, 0);
        if (n > 0) {
            got += (size_t)n;
            // Got some data.
            continue;
        }
        if (n == 0) {
            // peer closed during read
            return -3;
        }
        // n < 0
        if (errno == EINTR)
        {
            // I left this as warning to see if actually happens in real world.
            print_warning("WARNING: recv EINTR, retrying...\n");
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            if (do_not_wait_first_byte && got == 0) {
                // We do not want to wait for the first byte.
                return -1;
            }
            if (timeout_ms >= 0 && (now_ms() - start) >= timeout_ms) return -2; // timeout
            // Else we can do sleep here and try again.
            continue;
        }
        // real error
        return -4;
    }
    return 0;
}

int handle_low_level_recv_errors(int recv_status)
{
    switch (recv_status)
    {
    case -2:
        print_failure("Timeout reached during recv_exact_or_timeout\n");
        break;
    case -3:
        print_failure("Peer closed connection during read\n");
        break;
    case -4:
        print_failure("Other recv failure (check errno)\n");
        break;
    default:
        break;
    }
    return recv_status;
}

// Try to send exact data with some timeout in milliseconds or fail.
// Returns:
//  0 - success
// -2 - timeout
// -3 - send returned 0 bytes sent (should not happen)
// -4 - other send error (check errno)
int send_exact_or_timeout(int fd, const void *buf, size_t len, int timeout_ms) {
    const uint8_t *p = (const uint8_t*)buf;
    size_t sent = 0;
    int64_t start = now_ms();

    while (sent < len) {
        ssize_t n = send(fd, p + sent, len - sent, 0);
        if (n > 0) {
            sent += (size_t)n;
            // Sent some data.
            continue;
        }
        if (n == 0) {
            // This should not happen for send.
            return -3;
        }
        // n < 0
        if (errno == EINTR)
        {
            // I left this as warning to see if actually happens in real world.
            print_warning("WARNING: send EINTR, retrying...\n");
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            if (timeout_ms >= 0 && (now_ms() - start) >= timeout_ms) {
                print_failure("Timeout reached during send_exact_or_timeout\n");
                return -2; // timeout
            }
            // Else we can do sleep here and try again.
            continue;
        }
        // real error
        return -4;
    }
    return 0;
}

int handle_low_level_send_errors(int send_status)
{
    switch (send_status)
    {
    case -2:
        print_failure("Timeout reached during send_exact_or_timeout\n");
        break;
    case -3:
        print_failure("Send returned 0 bytes sent (should not happen)\n");
        break;
    case -4:
        print_failure("Other send failure (check errno)\n");
        break;
    default:
        break;
    }
    return send_status;
}

struct GenericPacket *receive_generic_packet(int socket)
{
    // WARNING: Worst case scenario may take 500ms due to timeouts.
    int receiving_status = 0;
    struct GenericPacket *received_packet = malloc(sizeof(struct GenericPacket));
    receiving_status = recv_exact_or_timeout(socket, &received_packet->sequence_number, sizeof(received_packet->sequence_number), 100, 1);
    if (handle_low_level_recv_errors(receiving_status))
    {
        free(received_packet);
        return NULL;
    }
    receiving_status = recv_exact_or_timeout(socket, &received_packet->packet_content_size, sizeof(received_packet->packet_content_size), 100, 0);
    if (handle_low_level_recv_errors(receiving_status))
    {
        free(received_packet);
        return NULL;
    }
    receiving_status = recv_exact_or_timeout(socket, &received_packet->packet_type, sizeof(received_packet->packet_type), 100, 0);
    if (handle_low_level_recv_errors(receiving_status))
    {
        free(received_packet);
        return NULL;
    }
    receiving_status = recv_exact_or_timeout(socket, &received_packet->checksum, sizeof(received_packet->checksum), 100, 0);
    if (handle_low_level_recv_errors(receiving_status))
    {
        free(received_packet);
        return NULL;
    }
    size_t to_receive = received_packet->packet_content_size;
    if (to_receive > MAX_CONTENT_SIZE)
    {
        print_failure("Received content size definition seems too large: ");
        printf("Received %ld bytes yet Max allowed is %d bytes\n", to_receive, MAX_CONTENT_SIZE);
        printf("^Packet^ info: SeqNr: %d Type: %d Checksum: %d\n", received_packet->sequence_number, received_packet->packet_type, received_packet->checksum);
        free(received_packet);
        return NULL;
    }
    received_packet->content = malloc(to_receive);
    receiving_status = recv_exact_or_timeout(socket, received_packet->content, to_receive, 100, 0);
    if (handle_low_level_recv_errors(receiving_status))
    {
        free(received_packet->content);
        free(received_packet);
        return NULL;
    }
    return received_packet;
}

int send_generic_packet(int socket, struct GenericPacket *packet)
{
    // WARNING: Worst case scenario may take 500ms due to timeouts.
    int sending_status = 0;
    sending_status = send_exact_or_timeout(socket, &packet->sequence_number, sizeof(packet->sequence_number), 100);
    if (handle_low_level_send_errors(sending_status))
    {
        return sending_status;
    }
    sending_status = send_exact_or_timeout(socket, &packet->packet_content_size, sizeof(packet->packet_content_size), 100);
    if (handle_low_level_send_errors(sending_status))
    {
        return sending_status;
    }
    sending_status = send_exact_or_timeout(socket, &packet->packet_type, sizeof(packet->packet_type), 100);
    if (handle_low_level_send_errors(sending_status))
    {
        return sending_status;
    }
    sending_status = send_exact_or_timeout(socket, &packet->checksum, sizeof(packet->checksum), 100);
    if (handle_low_level_send_errors(sending_status))
    {
        return sending_status;
    }
    sending_status = send_exact_or_timeout(socket, packet->content, packet->packet_content_size, 100);
    if (handle_low_level_send_errors(sending_status))
    {
        return sending_status;
    }
    return 0;
}

void close_socket(int socket)
{
    int close_response = close(socket);
    printf("(Socket/FD closed!) Close response: %d For socket: %d\n", close_response, socket);
}
