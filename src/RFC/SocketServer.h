#pragma once
#ifdef ARDUINO
// clang-format off
// FreeRTOS has to be imported before AsyncTCP.h
#include "freertos/FreeRTOS.h"
#include <AsyncTCP.h>
// clang-format on
#include <inttypes.h>

#include <cstddef>

typedef struct {
    const char *ssid;
    const char *pswd;
} ServerCredentials;

class SocketServer {
   private:
    // SocketServer configuration
    const uint16_t pull_portno, push_portno;

    ServerCredentials *credentials;

    AsyncServer *pullServer;
    AsyncServer *pushServer;

    // handler for client's received data
    void (*handler)(size_t, uint8_t *);

    // singleton
    static SocketServer *socketServer;
    SocketServer(uint16_t t_pullport, uint16_t t_pushport,
                 void (*t_handler)(size_t, uint8_t *));

    void registerClient(AsyncClient *new_client, AsyncClient **current_client);
    void unregisterClient(AsyncClient *t_client);

   public:
    AsyncClient *pullClient;
    AsyncClient *pushClient;

    void begin();
    void connect2Wifi(ServerCredentials *t_credentials);
    void write2Client(AsyncClient *client, const char *buf, size_t size_buf);
    void printf2Client(AsyncClient *client, const char *format, ...);
    bool hasPushClient();

    static SocketServer *getServer(void);
    static void createServer(uint16_t t_pullport, uint16_t t_pushport,
                             void (*t_handler)(size_t, uint8_t *));
};
#endif
