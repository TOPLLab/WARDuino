//
// Created by Carlos Rojas Castillo on 17/01/2023.
//

#ifndef WARDUINO_SOCKETCOMMUNICATION_H
#define WARDUINO_SOCKETCOMMUNICATION_H
#include <stdint.h>
// clang-format off
#include "freertos/FreeRTOS.h"
// clang-format on
#include <AsyncTCP.h>
#include <stdio.h>

struct WiFiCredentials {
    const char *ssid;
    const char *pswd;
};

class SocketCommunication {
   private:
    AsyncServer server;
    AsyncClient *client{};
    const char *name{};

    void (*onNewClient)(AsyncClient *c){};
    void (*onGoneClient)(){};
    void (*onData)(uint8_t *data, size_t len){};

   public:
    SocketCommunication(const char *t_name, uint16_t t_port,
                        void (*t_onData)(uint8_t *data, size_t len),
                        void (*t_onNewClient)(AsyncClient *c),
                        void (*t_onGoneClient)());

    void registerClient(AsyncClient *t_client);
    void unregisterClient(AsyncClient *t_client);
    void registerCallbacks(AsyncClient *t_client);
    void begin();

    static void connect2Wifi(struct WiFiCredentials credentials);
};

/*
class SocketCommunication {
   private:
    AsyncServer serverForTools;
    AsyncServer serverForVMClients;
    AsyncClient *toolClient{};
    AsyncClient *proxyClient{};

    void (*onNewProxyClient)(AsyncClient *c){};
    void (*onNewToolClient)(AsyncClient *c){};
    void (*onGoneProxyClient)(){};
    void (*onGoneToolClient)(){};

    void beginToolsServer();
    void beginVMServer();

   public:
    SocketCommunication(uint16_t t_porttools, uint16_t t_portvm,
                        void (*t_onNewProxyClient)(AsyncClient *c),
                        void (*t_onNewToolClient)(AsyncClient *c),
                        void (*t_onGoneProxyClient)(),
                        void (*t_onGoneToolClient)());

    void registerProxyClient(AsyncClient *t_client);
    void registerToolClient(AsyncClient *t_client);
    void unregisterToolsClient(AsyncClient *t_client);
    void unregisterProxyClient(AsyncClient *t_client);
    void registerCallbacks(AsyncClient *t_client);
    void begin();
};
*/

#endif  // WARDUINO_SOCKETCOMMUNICATION_H
