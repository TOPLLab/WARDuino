#include "sockets.h"

#ifndef __ZEPHYR__
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif
#include <unistd.h>

#ifndef __ZEPHYR__
#include <csignal>
#endif
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifndef __ZEPHYR__
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

struct sockaddr_in createLocalhostAddress(int port) {
    struct sockaddr_in address = createAddress(port);
    const char hostname[] = "localhost";
    struct hostent *resolvedhost = gethostbyname(hostname);
    memcpy(&address.sin_addr, resolvedhost->h_addr_list[0],
           resolvedhost->h_length);
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
#endif

Sink::Sink(FILE *out) {
    this->outStream = out;
    this->outDescriptor = fileno(out);
}

int Sink::write(const char *fmt, ...) const {
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

int FileDescriptorChannel::write(const char *fmt, ...) const {
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
    struct sockaddr_in address = createAddress(this->port);
    bindSocketToAddress(this->fileDescriptor, address);
    startListening(this->fileDescriptor);
    printf("Listening on port 127.0.0.1:%i\n", this->port);
    fflush(stdout);

    // block until a connection is established
    this->socket = listenForIncomingConnection(this->fileDescriptor, address);
}

void ClientSocket::open() {
    // bind socket to address
    this->fileDescriptor = createSocketFileDescriptor();
    struct sockaddr_in address = createAddress(this->port);  // server port
    if (connect(this->fileDescriptor, (struct sockaddr *)&address,
                sizeof(address)) < 0) {
        perror("Failed to connect to socket");
        exit(EXIT_FAILURE);
    }

    this->socket = this->fileDescriptor;
}

int WebSocket::write(const char *fmt, ...) const {
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
    struct sigaction sact {};
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
