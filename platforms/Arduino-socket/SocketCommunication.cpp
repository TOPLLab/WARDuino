//
// Created by Carlos Rojas Castillo on 17/01/2023.
//

#include "SocketCommunication.h"

#include <WiFi.h>

SocketCommunication::SocketCommunication(const char *t_name, uint16_t t_port,
                                         void (*t_onData)(uint8_t *data,
                                                          size_t len),
                                         void (*t_onNewClient)(AsyncClient *c),
                                         void (*t_onGoneClient)())
    : name(t_name),
      onData(t_onData),
      server(AsyncServer(t_port)),
      onNewClient(t_onNewClient),
      onGoneClient(t_onGoneClient) {}

void SocketCommunication::begin() {
    this->server.onClient(
        [this](void *s, AsyncClient *c) {
            printf("%s: Client connected!\n", this->name);
            if (c == nullptr) {
                printf("%s: Client is nullptr\n", this->name);
                return;
            }
            this->registerClient(c);
            this->registerCallbacks(c);
        },
        NULL);
}

void SocketCommunication::registerClient(AsyncClient *t_client) {
    if (this->client != nullptr) {
        printf("%s: Only one Client allowed. Closing new client!\n",
               this->name);
        t_client->close(true);
        t_client->free();
        delete t_client;
        return;
    }
    this->client = t_client;
    this->onNewClient(t_client);
}

void SocketCommunication::registerCallbacks(AsyncClient *t_client) {
    t_client->onError(
        [this](void *r, AsyncClient *t_client, int8_t error) {
            printf("%s: Error on client %" PRIu8 "!\n", this->name, error);
            this->unregisterClient(t_client);
        },
        NULL);
    t_client->onDisconnect(
        [this](void *r, AsyncClient *t_client) {
            printf("%s: Client disconnected!\n", this->name);
            this->unregisterClient(t_client);
        },
        NULL);
    t_client->onTimeout(
        [this](void *r, AsyncClient *t_client, uint32_t time) {
            printf("%s: Client timedout %" PRIu32 "!\n", this->name, time);
            this->unregisterClient(t_client);
        },
        NULL);
    t_client->onData([this](void *r, AsyncClient *t_client, void *buf,
                            size_t len) { this->onData((uint8_t *)buf, len); },
                     NULL);
}

void SocketCommunication::unregisterClient(AsyncClient *t_client) {
    if (this->client == t_client) {
        printf("%s: Closing Client\n", this->name);
        t_client->close(true);
        t_client->free();
        this->client = nullptr;
        this->onGoneClient();
        delete t_client;
        return;
    }

    printf("%s: Closing an unknown Client\n", this->name);
    t_client->close(true);
    t_client->free();
    delete t_client;
}

void SocketCommunication::connect2Wifi(struct WiFiCredentials credentials) {
    printf("Connecting to WiFi..\n\n");
    WiFi.begin(credentials.ssid, credentials.pswd);
    while (WiFi.status() != WL_CONNECTED) {
        delay(10);
    }

    printf("%d.%d.%d.%d\n\n", WiFi.localIP()[0], WiFi.localIP()[1],
           WiFi.localIP()[2], WiFi.localIP()[3]);
}

// SocketCommunication::SocketCommunication(
//     uint16_t t_porttools, uint16_t t_portvm,
//     void (*t_onNewProxyClient)(AsyncClient *c),
//     void (*t_onNewToolClient)(AsyncClient *c), void (*t_onGoneProxyClient)(),
//     void (*t_onGoneToolClient)())
//     : serverForTools(AsyncServer(t_porttools)),
//       serverForVMClients(AsyncSever(t_portvm)),
//       onNewProxyClient(t_onNewProxyClient),
//       onNewToolClient(t_onNewToolClient),
//       onGoneProxyClient(t_onGoneProxyClient),
//       onGoneToolClient(t_onGoneToolClient) {}
//
// void SocketCommunication::begin() {
//     this->beginToolsServer();
//     this->beginVMServer();
// }
//
// void SocketCommunication::beginToolsServer() {
//     this->serverForTools.begin();
//     this->serverForTools.onClient(
//         [this](void *s, AsyncClient *c) {
//             printf("A tool client connected!\n");
//             if (c == nullptr) {
//                 printf("tool Client is nullptr\n");
//                 return;
//             }
//             this->registerToolClient(c);
//             this->registerCallbacks(c);
//         },
//         NULL);
// }
//
// void SocketCommunication::beginVMServer() {
//     this->serverForVMClients.begin();
//     this->serverForVMClients.onClient(
//         [this](void *s, AsyncClient *c) {
//             printf("A VM client connected!\n");
//             if (c == nullptr) {
//                 printf("VM Client is nullptr\n");
//                 return;
//             }
//             this->registerProxyClient(c);
//             this->registerCallbacks(c);
//         },
//         NULL);
// }
//
// void SocketCommunication::registerProxyClient(AsyncClient *t_client) {
//     if (this->proxyClient != nullptr) {
//         printf("Only one VM Client allowed. Closing new client\n");
//         t_client->close(true);
//         t_client->free();
//         this->onGoneProxyClient();
//         delete t_client;
//         return;
//     }
//     this->toolClient = t_client;
//     this->onNewProxyClient(t_client);
// }
//
// void SocketCommunication::registerToolClient(AsyncClient *t_client) {
//     if (this->toolClient != nullptr) {
//         printf("Only one tool Client allowed. Closing new client\n");
//         t_client->close(true);
//         t_client->free();
//         delete t_client;
//         return;
//     }
//     this->toolClient = t_client;
//     this->onNewToolClient(toolClient);
// }
//
// void SocketCommunication::registerCallbacks(AsyncClient *t_client) {
//     SocketCommunication *thisServer = this;
//     t_client->onError(
//         [thisServer](void *r, AsyncClient *t_client, int8_t error) {
//             printf("ClientSocket Error %" PRIu8 "\n", error);
//         },
//         NULL);
//     t_client->onDisconnect(
//         [thisServer](void *r, AsyncClient *t_client) {
//             printf("Client Disconnected\n");
//             thisServer->unregisterClient(t_client);
//         },
//         NULL);
//     t_client->onTimeout(
//         [thisServer](void *r, AsyncClient *t_client, uint32_t time) {
//             printf("Client timeouted\n");
//             thisServer->unregisterClient(t_client);
//         },
//         NULL);
//     t_client->onData(
//         [wrd](void *r, AsyncClient *t_client, void *buf, size_t len) {
//             wrd->handleInterrupt(len, (uint8_t *)buf);
//         },
//         NULL);
//     /* t_client->onAck([](void *r, AsyncClient* t_client, size_t len,
//     uint32_t
//      * time){ */
//     /*   printf("Ack: time \n"); */
//     /*   }, NULL); */
//     /* t_client->onPoll([](void *r, AsyncClient* t_client){ (void)c;}, NULL);
//     */
//     /* c->write("ping!\n"); */
// }
//
// void SocketCommunication::unregisterToolsClient(AsyncClient *t_client) {
//     if (this->proxyClient == t_client) {
//         printf("Only one VM Client allowed. Closing new client\n");
//         t_client->close(true);
//         t_client->free();
//         this->onGoneProxyClient();
//         delete t_client;
//         return;
//     }
//     if (this->)
//         t_client->close(true);  // TODO potential issue: close twice same
//         client
//     t_client->free();
//     delete t_client;
// }
//
// void SocketCommunication::unregisterProxyClient(AsyncClient *t_client) {
//     if (this->forIO == t_client) {
//         this->forIO = nullptr;
//     } else if (this->forEvents == t_client) {
//         this->forEvents = nullptr;
//     } else if (this->forProxy == t_client) {
//         this->forProxy = nullptr;
//     }
//     t_client->close(true);  // TODO potential issue: close twice same client
//     t_client->free();
//     delete t_client;
// }
