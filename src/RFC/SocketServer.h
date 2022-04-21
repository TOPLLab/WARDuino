#pragma once
#ifdef ARDUINO
#include <AsyncTCP.h>
#include <FreeRTOS.h>  //FreeRTOS has to be imported before AsyncTCP.h
#include <inttypes.h>

#include <cstddef>

typedef struct {
    const char *ssid;
    const char *pswd;
} ServerCredentials;

class SocketServer {
   private:
    // SocketServer configuration
    const uint16_t portno;
    ServerCredentials *credentials;

    AsyncServer *asyncServer;
    AsyncClient *client;

    // handler for client's received data
    void (*handler)(size_t, uint8_t *);

    // singleton
    static SocketServer *socketServer;
    SocketServer(uint16_t t_port, void (*t_handler)(size_t, uint8_t *));

    void registerClient(AsyncClient *t_client);
    void unregisterClient(AsyncClient *t_client);

   public:
    void begin();
    void connect2Wifi(ServerCredentials *t_credentials);
    void write2Client(const char *buf, size_t size_buf);

    static SocketServer *getServer(void);
    static void initializeServer(uint16_t t_port,
                                 void (*t_handler)(size_t, uint8_t *));
};
#endif
