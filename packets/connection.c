#include "connection.h"
#include <sys/socket.h>     // socket(), AF_INET, SOCK_STREAM
#include <netinet/ip.h>     // sockaddr_in struct
#include <stdio.h>          // perror()
#include <stdlib.h>         // exit(), size_t, malloc
#include <string.h>         // memset()
#include <unistd.h>         // close()
#include "shared_packets.h" // packets used by battleships
#include <arpa/inet.h>      // inet_pton()

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
    printf("Waiting for a new connection for socket: %d\n", socket);
    int client_socket = accept(socket, (struct sockaddr *)&client_address, &client_socket_len);
    if (client_socket < 0)
    {
        perror("Error: ");
    }
    return client_socket;
}

struct GenericPacket *receive_generic_packet(int socket)
{
    // Call free(address) for this one or enjoy memory leak 😍
    struct GenericPacket *received_packet = malloc(sizeof(struct GenericPacket));
    size_t recv_status, to_receive;
    printf("Receiving generic packet:\n");
    // This and all further to_receive sizeof tells recv how buch bytes to receive.
    // uint32_t sequence_number;     // 4 bytes
    to_receive = sizeof(received_packet->sequence_number);
    recv_status = recv(socket, &received_packet->sequence_number, to_receive, 0);
    if (recv_status != to_receive)
    {
        printf("Failure to get packet sequence number! Received: %ld bytes Expected: %ld bytes\n", recv_status, to_receive);
    }
    // uint32_t packet_content_size; // 4 bytes
    to_receive = sizeof(received_packet->packet_content_size);
    recv_status = recv(socket, &received_packet->packet_content_size, to_receive, 0);
    if (recv_status != to_receive)
    {
        printf("Failure to get packet content size! Received: %ld bytes Expected: %ld bytes\n", recv_status, to_receive);
    }
    // uint8_t packet_type;          // 1 byte
    to_receive = sizeof(received_packet->packet_type);
    recv_status = recv(socket, &received_packet->packet_type, to_receive, 0);
    if (recv_status != to_receive)
    {
        printf("Failure to get packet type! Received: %ld bytes Expected: %ld bytes\n", recv_status, to_receive);
    }
    // uint8_t checksum;             // 1 byte
    to_receive = sizeof(received_packet->checksum);
    recv_status = recv(socket, &received_packet->checksum, to_receive, 0);
    if (recv_status != to_receive)
    {
        printf("Failure to get packet content size! Received: %ld bytes Expected: %ld bytes\n", recv_status, to_receive);
    }
    // char *content; // Content is saved somewhere on memory
    to_receive = received_packet->packet_content_size;
    if (to_receive > MAX_CONTENT_SIZE)
    {
        printf("Received content size definition seems too big: %ld bytes Max allowed is: %d bytes\n", to_receive, MAX_CONTENT_SIZE);
        received_packet->packet_content_size = 0;
        return received_packet;
    }
    // Call free(address) for this one or enjoy memory leak 😍
    received_packet->content = malloc(to_receive);
    recv_status = recv(socket, received_packet->content, to_receive, 0);
    if (recv_status != to_receive)
    {
        printf("Received content size seems off. Received: %ld bytes Expected: %ld bytes\n", to_receive, recv_status);
    }
    return received_packet;
}

int send_generic_packet(int socket, struct GenericPacket *packet)
{
    size_t send_status, to_send;
    // uint32_t sequence_number;     // 4 bytes
    to_send = sizeof(packet->sequence_number);
    send_status = send(socket, &packet->sequence_number, to_send, 0);
    if (send_status != to_send)
    {
        printf("Failure to send packet sequence number! Sent: %ld bytes Had to send: %ld bytes\n", send_status, to_send);
        return 1;
    }
    // uint32_t packet_content_size; // 4 bytes
    to_send = sizeof(packet->packet_content_size);
    send_status = send(socket, &packet->packet_content_size, to_send, 0);
    if (send_status != to_send)
    {
        printf("Failure to send packet content size! Sent: %ld bytes Had to send: %ld bytes\n", send_status, to_send);
        return 2;
    }
    // uint8_t packet_type;          // 1 byte
    to_send = sizeof(packet->packet_type);
    send_status = send(socket, &packet->packet_type, to_send, 0);
    if (send_status != to_send)
    {
        printf("Failure to send packet type! Sent: %ld bytes Had to send: %ld bytes\n", send_status, to_send);
        return 3;
    }
    // uint8_t checksum;             // 1 byte
    to_send = sizeof(packet->checksum);
    send_status = send(socket, &packet->checksum, to_send, 0);
    if (send_status != to_send)
    {
        printf("Failure to send packet sequence number! Sent: %ld bytes Had to send: %ld bytes\n", send_status, to_send);
        return 4;
    }
    // char *content; // Content is saved somewhere on memory
    to_send = packet->packet_content_size;
    send_status = send(socket, packet->content, to_send, 0);
    if (send_status != to_send)
    {
        printf("Sent content seems off: %ld bytes Expected: %ld bytes\n", to_send, send_status);
        return 5;
    }
    return 0;
}

void close_socket(int socket)
{
    int close_response = close(socket);
    printf("(Socket/FD closed!) Close response: %d For socket: %d\n", close_response, socket);
}
