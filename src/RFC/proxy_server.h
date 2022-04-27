#pragma once

#include <cinttypes>

struct Address;

class ProxyServer {
   private:
    // for singleton
    static ProxyServer *proxyServer;

    char *host;
    int pull_port, push_port, pull_socket, push_socket;

    struct Address *address;

    // private constructor for singleton
    ProxyServer();

    void startPushDebuggerSocket() const;

   public:
    char *exceptionMsg;

    void registerAddress(char *t_host, int t_port);
    void closeConnections();
    bool openConnections();
    void updateExcpMsg(const char *msg);
    bool send(void *t_buffer, int t_size);
    char *readReply(short int amount = 1024);

    static void registerMCUHost(uint8_t **data);
    static ProxyServer *getServer();
};
