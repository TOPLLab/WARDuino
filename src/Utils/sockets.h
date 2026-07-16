#pragma once

#include <unistd.h>

#include <cstdio>

#ifdef __ZEPHYR__
#if IS_ENABLED(CONFIG_WIFI)
#include <sys/socket.h>  // Zephyr renames sockaddr_in with macros
#define WIFI_ENABLED
#endif
#else
#define WIFI_ENABLED
#endif

int setFileDescriptorOptions(int socket_fd);

int createSocketFileDescriptor();

int bindSocketToAddress(int socket_fd, struct sockaddr_in address);

struct sockaddr_in createServerAddress(int port);

struct sockaddr_in createLocalhostAddress(int port);

int startListening(int socket_fd);

int listenForIncomingConnection(int socket_fd, struct sockaddr_in address);

class Channel {
   public:
    virtual void open() {}
    virtual int write(char const *, ...) { return 0; }
    virtual ssize_t read(void *, size_t) { return 0; }
    virtual void close() {}
    virtual ~Channel() = default;
};

// note: Channel::write could be a const function, but since it's a proxy
// for writing to an output stream, it shouldn't be const (this also eliminates
// a lot of "expression result unused" warnings).

class Sink : public Channel {
   private:
    FILE *outStream;

   public:
    explicit Sink(FILE *out);
    int write(char const *fmt, ...) override;
};

class Duplex : public Sink {
   private:
    int inDescriptor;

   public:
    explicit Duplex(FILE *inStream, FILE *outStream);

    ssize_t read(void *out, size_t size) override;
};

class FileDescriptorChannel : public Channel {
   private:
    int fd;

   public:
    explicit FileDescriptorChannel(int fileDescriptor);

    int write(char const *fmt, ...) override;
    ssize_t read(void *out, size_t size) override;
};

class WebSocket : public Channel {
   protected:
    int port;
    int fileDescriptor;
    int socket;

   public:
    explicit WebSocket(int port);

    void open() override;
    int write(char const *fmt, ...) override;
    ssize_t read(void *out, size_t size) override;
    void close() override;
};

class ClientSocket : public WebSocket {
   public:
    explicit ClientSocket(int server);
    void open() override;
};