#pragma once
#ifdef ARDUINO
#include <cstddef>
#include <FreeRTOS.h>
#include <AsyncTCP.h>
#include <inttypes.h>

#include "WARDuino.h"

typedef struct {
    const char *ssid;
    const char *pswd;
} ServerCredentials;

class SocketServer {
   private:
    AsyncServer *asyncServer;
    AsyncClient *client;
    WARDuino *warduino;

    const uint16_t portno;
    ServerCredentials *credentials;

    void registerClient(AsyncClient *t_client);
    void unregisterClient(AsyncClient *t_client);

   public:
    // singleton
    static SocketServer *socketServer;
    SocketServer(uint16_t t_port, WARDuino *t_wrd);

    void begin();
    void connect2Wifi(ServerCredentials *t_credentials);
    void write2Client(const char *buf, size_t size_buf);

    static SocketServer *getServer(void);
    static void initializeServer(uint16_t t_port, WARDuino *t_wrd);
};
#endif
