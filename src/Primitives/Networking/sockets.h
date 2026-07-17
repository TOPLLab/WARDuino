#pragma once

#include <arpa/inet.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "../../Utils/sockets.h"

namespace sockets {
inline int socket_create(const char *ip, const int32_t port) {
    printf("Create socket %s:%d\n", ip, port);
    const int sock = createSocketFileDescriptor();
    if (sock < 0) {
        return -1;
    }

    sockaddr_in server_addr = {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip);

    if (connect(sock, reinterpret_cast<sockaddr *>(&server_addr),
                sizeof(server_addr)) < 0) {
        printf("Failed to connect %s\n", strerror(errno));
        close(sock);
        return -1;
    }
    printf("Connected to %s:%d\n", ip, port);
    printf("sock = %d\n", sock);
    return sock;
}

inline int socket_create_server(const int32_t port) {
    const int sock = createSocketFileDescriptor();
    if (sock < 0) {
        return -1;
    }
    const sockaddr_in addr = createServerAddress(port);
    if (bindSocketToAddress(sock, addr) < 0) {
        return -1;
    }
    if (startListening(sock) < 0) {
        return -1;
    }

    printf("Server listening on port %d (sock=%d)\n", port, sock);
    return sock;
}

inline int socket_accept(const int socket) {
    sockaddr_in client_addr = {};
    socklen_t client_addr_len = sizeof(client_addr);
    printf("Waiting for connection on sock=%d\n", socket);
    const int client_sock = accept(
        socket, reinterpret_cast<sockaddr *>(&client_addr), &client_addr_len);
    if (client_sock < 0) {
        printf("error: accept failed: %s\n", strerror(errno));
        return -1;
    }
    printf("Client connected (client_sock=%d)\n", client_sock);
    return client_sock;
}

inline int socket_send(const int socket, const char *message,
                       const size_t len) {
    printf("socket_send(%d, \"%s\" (len = %d))\n", socket, message,
           len);
    return send(socket, message, len, 0);
}

inline int socket_receive(const int socket, char *buffer, const size_t size) {
    printf("socket_receive(%d, %p, %lu)\n", socket, buffer, size);
    return recv(socket, buffer, size, 0);
}

inline int socket_close(int socket) {
    printf("socket_close(%d)\n", socket);
    // Wait a bit to make sure any sent messages in the buffer are still sent.
    timespec ts = {};
    ts.tv_sec = 0;
    ts.tv_nsec = 500 * 1000000L;
    nanosleep(&ts, nullptr);
    return close(socket);
}
}  // namespace sockets
