#include "sockets.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

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
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
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

struct sockaddr_in createRemoteHostAddress(const char *host, int port) {
    struct sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(host);
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

Sink::Sink(FILE *out) {
    this->outStream = out;
    this->outDescriptor = fileno(out);
}

int Sink::write(const char *fmt, ...) const {
    va_list args;
    va_start(args, fmt);
    int written = vdprintf(this->outDescriptor, fmt, args);
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

void WebSocket::open() {
    // bind socket to address
    this->fileDescriptor = createSocketFileDescriptor();
    struct sockaddr_in address = createAddress(this->port);
    bindSocketToAddress(this->fileDescriptor, address);
    startListening(this->fileDescriptor);
    printf("Listening on port 127.0.0.1:%i\n", this->port);

    // block until a connection is established
    this->socket = listenForIncomingConnection(this->fileDescriptor, address);
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

ClientSideSocket::ClientSideSocket(const char *t_host, int t_port)
    : port(t_port), host(t_host) {}

void ClientSideSocket::open() {
    printf("ClientSideSocket: connecting to %s:%d\n", this->host, this->port);

    this->socketfd = createSocketFileDescriptor();
    struct sockaddr_in address =
        createRemoteHostAddress(this->host, this->port);
    if (connect(this->socketfd, (struct sockaddr *)&address, sizeof(address)) !=
        0) {
        printf("connection with the server failed...\n");
        exit(0);
    } else
        printf("connected to the server..\n");
}

int ClientSideSocket::write(char const *fmt, ...) const {
    va_list args;
    va_start(args, fmt);

    int strSize = std::vsnprintf(const_cast<char *>(this->sendBuffer),
                                 this->bufferSize, fmt, args);
    if (strSize >= this->bufferSize) {
        while (true) {
            printf("TOO MUCH\n");
        }
    }

    int n = ::write(this->socketfd, this->sendBuffer, strSize);
    va_end(args);
    return n;
}

ssize_t ClientSideSocket::read(void *out, size_t size) {
    return ::read(this->socketfd, out, size);
}

void ClientSideSocket::close() {}

#ifdef ARDUINO
#include <stdio.h>

ServerSideSocket::ServerSideSocket(AsyncClient *t_client) : client(t_client) {}

void ServerSideSocket::open(){};

int ServerSideSocket::write(char const *fmt, ...) const {
    va_list args;
    va_start(args, fmt);
    int written =
        std::vsnprintf(const_cast<char *>(this->sendBuffer), 1024, fmt, args);
    if (written >= 1024) {
        while (true) {
            printf("TOO MUCH\n");
        }
    }

    size_t total = (size_t)written;
    size_t offset = 0;
    while (offset < total) {
        while (!this->client->canSend()) {
            printf("SocketClient::write: looping cannot send to client yet\n");
        }
        size_t amountToAdd = (total - offset) > this->client->space()
                                 ? this->client->space()
                                 : (total - offset);
        this->client->add(this->sendBuffer + offset, amountToAdd);
        this->client->send();
        offset += amountToAdd;
    }
    va_end(args);
    return written;
}

ssize_t ServerSideSocket::read(void *out, size_t size) {
    while (true) {
        printf("Trying to read from client\n");
    }
}

void ServerSideSocket::close(){};
#endif