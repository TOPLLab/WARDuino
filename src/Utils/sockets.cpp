#include "sockets.h"

#ifdef WIFI_ENABLED
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef WIFI_ENABLED
// Socket Debugger Interface
int setFileDescriptorOptions(int socket_fd) {
    int opt = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("Failed to set socket file descriptor options");
        return -1;
    }
    return 0;
}

int createSocketFileDescriptor() {
    int socket_fd;
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        perror("Failed to make a new socket file descriptor");
        return -1;
    }
    if (setFileDescriptorOptions(socket_fd) < 0) {
        return -1;
    }
    return socket_fd;
}

int bindSocketToAddress(const int socket_fd, sockaddr_in address) {
    if (bind(socket_fd, reinterpret_cast<sockaddr *>(&address),
             sizeof(address)) < 0) {
        perror("Binding socket to address failed");
        close(socket_fd);
        return -1;
    }
    return 0;
}

sockaddr_in createServerAddress(int port) {
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    return address;
}

int startListening(const int socket_fd) {
    if (listen(socket_fd, 1) < 0) {
        perror("listen");
        close(socket_fd);
        return -1;
    }
    return 0;
}

int listenForIncomingConnection(const int socket_fd, sockaddr_in address) {
    int new_socket;
    int size = sizeof(address);
    if ((new_socket = accept(socket_fd, reinterpret_cast<sockaddr *>(&address),
                             reinterpret_cast<socklen_t *>(&size))) < 0) {
        perror("Failed to listen for incoming connections");
        exit(EXIT_FAILURE);
    }
    return new_socket;
}

#endif

#ifndef __ZEPHYR__
sockaddr_in createLocalhostAddress(int port) {
    sockaddr_in address = createServerAddress(port);
    constexpr char hostname[] = "localhost";
    const hostent *resolvedhost = gethostbyname(hostname);
    memcpy(&address.sin_addr, resolvedhost->h_addr_list[0],
           resolvedhost->h_length);
    return address;
}
#endif

Sink::Sink(FILE *out) { this->outStream = out; }

int Sink::write(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int written = vfprintf(this->outStream, fmt, args);
    va_end(args);
    fflush(this->outStream);
    return written;
}

Duplex::Duplex(FILE *inStream, FILE *outStream) : Sink(outStream) {
    this->inDescriptor = fileno(inStream);
}

ssize_t Duplex::read(void *out, size_t size) {
    return ::read(this->inDescriptor, out, size);
}

#ifndef __ZEPHYR__
FileDescriptorChannel::FileDescriptorChannel(int fileDescriptor) {
    this->fd = fileDescriptor;
}

int FileDescriptorChannel::write(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int written = vdprintf(this->fd, fmt, args);
    va_end(args);
    return written;
}

ssize_t FileDescriptorChannel::read(void *out, size_t size) {
    return ::read(this->fd, out, size);
}

WebSocket::WebSocket(int port) {
    this->port = port;
    this->fileDescriptor = -1;
    this->socket = -1;
}

ClientSocket::ClientSocket(int server) : WebSocket(server) {}

void WebSocket::open() {
    // bind socket to address
    this->fileDescriptor = createSocketFileDescriptor();
    if (this->fileDescriptor < 0) {
        exit(EXIT_FAILURE);
    }
    const sockaddr_in address = createServerAddress(this->port);
    if (bindSocketToAddress(this->fileDescriptor, address) < 0) {
        exit(EXIT_FAILURE);
    }
    if (startListening(this->fileDescriptor) < 0) {
        exit(EXIT_FAILURE);
    }
    printf("Listening on port 127.0.0.1:%i\n", this->port);
    fflush(stdout);

    // block until a connection is established
    this->socket = listenForIncomingConnection(this->fileDescriptor, address);
}

void ClientSocket::open() {
    // bind socket to address
    this->fileDescriptor = createSocketFileDescriptor();
    sockaddr_in address = createServerAddress(this->port);  // server port
    if (connect(this->fileDescriptor, reinterpret_cast<sockaddr *>(&address),
                sizeof(address)) < 0) {
        perror("Failed to connect to socket");
        exit(EXIT_FAILURE);
    }

    this->socket = this->fileDescriptor;
}

int WebSocket::write(const char *fmt, ...) {
    if (this->socket < 0) {
        return 0;
    }
    va_list args;
    va_start(args, fmt);
    int written = vdprintf(this->socket, fmt, args);
    va_end(args);
    return written;
}

ssize_t WebSocket::read(void *out, size_t size) {
    if (this->socket < 0) {
        return 0;
    }
    return ::read(this->socket, out, size);
}

void sendAlarm() {
    struct sigaction sact{};
    sigemptyset(&sact.sa_mask);
    sact.sa_flags = 0;
    sigaction(SIGALRM, &sact, nullptr);
    alarm(0);
}

void WebSocket::close() {
    sendAlarm();  // stop possible blocking accept call
    shutdown(this->fileDescriptor, SHUT_RDWR);  // shutdown connection
}
#endif
