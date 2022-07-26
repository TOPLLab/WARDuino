#pragma once

#include <cstdio>
void setFileDescriptorOptions(int socket_fd);

int createSocketFileDescriptor();

void bindSocketToAddress(int socket_fd, struct sockaddr_in address);

struct sockaddr_in createAddress(int port);

void startListening(int socket_fd);

int listenForIncomingConnection(int socket_fd, struct sockaddr_in address);

class Channel {
   private:
    int socket;

   public:
    explicit Channel(int socket);

    int write(char const *fmt, ...) const;
    ssize_t read(void *out, size_t size);
};