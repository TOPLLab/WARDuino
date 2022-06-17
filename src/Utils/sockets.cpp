#include "sockets.h"

#include <netinet/in.h>
#include <sys/socket.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

// Socket Debugger Interface
void setFileDescriptorOptions(int socket_fd) {
    int opt = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("Failed to set socket file descriptor options");
        exit(EXIT_FAILURE);
    }
}

int createSocketFileDescriptor() {
    int socket_fd;
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Failed to make a new socket file descriptor");
        exit(EXIT_FAILURE);
    }
    setFileDescriptorOptions(socket_fd);
    return socket_fd;
}

void bindSocketToAddress(int socket_fd, struct sockaddr_in address) {
    if (bind(socket_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Binding socket to address failed");
        exit(EXIT_FAILURE);
    }
}

struct sockaddr_in createAddress(int port) {
    struct sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    return address;
}

void startListening(int socket_fd) {
    if (listen(socket_fd, 1) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
}

int listenForIncomingConnection(int socket_fd, struct sockaddr_in address) {
    int new_socket;
    int size = sizeof(address);
    if ((new_socket = accept(socket_fd, (struct sockaddr *)&address,
                             (socklen_t *)&size)) < 0) {
        perror("Failed to listen for incoming connections");
        exit(EXIT_FAILURE);
    }
    return new_socket;
}

Channel::Channel(int socket) { this->socket = socket; }

void Channel::write(const char *fmt, ...) const {
    va_list args;
    va_start(args, fmt);
    vdprintf(this->socket, fmt, args);
    va_end(args);
}
