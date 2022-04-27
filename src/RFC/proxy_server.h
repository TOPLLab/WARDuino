#pragma once

#include <inttypes.h>

struct Address;

class ProxyServer {
   private:
    // for singleton
    static ProxyServer *proxyServer;

    char *host;
    int port, sockfd;

    struct Address *address;

    // private constructor for singleton
    ProxyServer();

   public:
    char *exceptionMsg;

    void registerAdress(char *t_host, int t_port);
    void closeConnection(void);
    bool openConnection(void);
    void updateExcpMsg(const char *msg);
    bool send(void *t_buffer, int t_size);
    char *readReply(short int amount = 1024);

    static void registerMCUHost(uint8_t **data);
    static ProxyServer *getServer(void);
};
