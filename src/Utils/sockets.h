#pragma once

#include <cstdio>
void setFileDescriptorOptions(int socket_fd);

int createSocketFileDescriptor();

void bindSocketToAddress(int socket_fd, struct sockaddr_in address);

struct sockaddr_in createAddress(int port);

struct sockaddr_in createLocalhostAddress(int port);

void startListening(int socket_fd);

int listenForIncomingConnection(int socket_fd, struct sockaddr_in address);

class Channel {
   public:
    virtual void open() {}
    virtual int write(char const *fmt, ...) const {}
    virtual ssize_t read(void *out, size_t size) {}
    virtual void close() {}
    virtual ~Channel() = default;
};

class FileChannel : public Channel {
   private:
    FILE *outStream;
    int outDescriptor;
    int inDescriptor;

   public:
    explicit FileChannel(FILE *in, FILE *out);

    int write(char const *fmt, ...) const override;
    ssize_t read(void *out, size_t size) override;
};

class FileDescriptorChannel : public Channel {
   private:
    int fd;

   public:
    explicit FileDescriptorChannel(int fileDescriptor);

    int write(char const *fmt, ...) const override;
    ssize_t read(void *out, size_t size) override;
};

class WebSocket : public Channel {
   private:
    int port;
    int fileDescriptor;
    int socket;

   public:
    explicit WebSocket(int port);

    void open() override;
    int write(char const *fmt, ...) const override;
    ssize_t read(void *out, size_t size) override;
    void close() override;
};