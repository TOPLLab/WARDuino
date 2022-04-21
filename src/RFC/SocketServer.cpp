#ifdef ARDUINO
#include "SocketServer.h"

#include <WiFi.h>

#include "../Utils/macros.h"

SocketServer *SocketServer::socketServer = nullptr;

SocketServer::SocketServer(uint16_t t_port,
                           void (*t_handler)(size_t, uint8_t *))
    : portno(t_port) {
    this->asyncServer = new AsyncServer(t_port);
    this->handler = t_handler;
}

void SocketServer::createServer(uint16_t t_port,
                                void (*t_handler)(size_t, uint8_t *)) {
    if (socketServer == nullptr)
        socketServer = new SocketServer(t_port, t_handler);
}

SocketServer *SocketServer::getServer() { return socketServer; }

void SocketServer::connect2Wifi(ServerCredentials *t_credentials) {
    debug("Connecting to WiFi..");
    WiFi.begin(t_credentials->ssid, t_credentials->pswd);
    while (WiFi.status() != WL_CONNECTED) {
        delay(10);
    }

    debug("%d.%d.%d.%d\n\n", WiFi.localIP()[0], WiFi.localIP()[1],
          WiFi.localIP()[2], WiFi.localIP()[3]);
}

void SocketServer::begin() {
    this->asyncServer->begin();
    this->asyncServer->onClient(
        [this](void *s, AsyncClient *c) {
            debug("A new client connected!\n");
            this->registerClient(c);
        },
        NULL);
}

void SocketServer::registerClient(AsyncClient *t_client) {
    if (t_client == NULL) {
        debug("a new Client is NULL\n");
        return;
    }

    if (this->client == nullptr) {
        this->client = t_client;
    } else {
        debug("Only one socket client allowed.\n");
        t_client->close(true);
        t_client->free();
        delete t_client;
        return;
    }

    void (*handler)(size_t, uint8_t *) = this->handler;
    SocketServer *thisServer = this;
    t_client->onError(
        [thisServer](void *r, AsyncClient *t_client, int8_t error) {
            debug("ClientSocket Error %" PRIu8 "\n", error);
        },
        NULL);
    t_client->onDisconnect(
        [thisServer](void *r, AsyncClient *t_client) {
            debug("Client Disconnected\n");
            thisServer->unregisterClient(t_client);
        },
        NULL);
    t_client->onTimeout(
        [thisServer](void *r, AsyncClient *t_client, uint32_t time) {
            debug("Client timeouted\n");
            thisServer->unregisterClient(t_client);
        },
        NULL);
    t_client->onData([handler](void *r, AsyncClient *t_client, void *buf,
                               size_t len) { handler(len, (uint8_t *)buf); },
                     NULL);
}

void SocketServer::unregisterClient(AsyncClient *t_client) {
    if (this->client == t_client) {
        this->client = nullptr;
    }
    t_client->close(true);  // TODO potential issue: close twice same client
    t_client->free();
    delete t_client;
}

void SocketServer::write2Client(const char *buf, size_t size_buf) {
    if (this->client == nullptr) return;
    size_t space_left = client->space();
    client->add(buf, size_buf > space_left ? space_left : size_buf);
    client->send();
    if (size_buf <= space_left) return;
    write2Client(buf + space_left, size_buf - space_left);
}
#endif
