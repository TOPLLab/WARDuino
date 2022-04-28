#ifdef ARDUINO
#include "SocketServer.h"

#include <WiFi.h>

#include "../Utils/macros.h"

SocketServer *SocketServer::socketServer = nullptr;

SocketServer::SocketServer(uint16_t t_pullport, uint16_t t_pushport,
                           void (*t_handler)(size_t, uint8_t *))
    : pull_portno(t_pullport), push_portno(t_pushport) {
    this->pullServer = new AsyncServer(t_pullport);
    this->pushServer = new AsyncServer(t_pushport);
    this->pullClient = nullptr;
    this->pushClient = nullptr;
    this->handler = t_handler;
}

void SocketServer::createServer(uint16_t t_pullport, uint16_t t_pushport,
                                void (*t_handler)(size_t, uint8_t *)) {
    if (socketServer == nullptr)
        socketServer = new SocketServer(t_pullport, t_pushport, t_handler);
}

SocketServer *SocketServer::getServer() { return socketServer; }

void SocketServer::connect2Wifi(ServerCredentials *t_credentials) {
    printf("Connecting to WiFi...\n");
    WiFi.begin(t_credentials->ssid, t_credentials->pswd);
    while (WiFi.status() != WL_CONNECTED) {
        delay(10);
    }

    printf("localip: %d.%d.%d.%d\n\n", WiFi.localIP()[0], WiFi.localIP()[1],
           WiFi.localIP()[2], WiFi.localIP()[3]);
}

void SocketServer::begin() {
    printf("starting PullSever\n");
    SocketServer *thisServer = this;
    this->pullServer->begin();
    this->pullServer->onClient(
        [thisServer](void *s, AsyncClient *c) {
            printf("A new PullClient connected!\n");
            thisServer->registerClient(c, &thisServer->pullClient);
        },
        NULL);

    printf("starting PushSever\n");
    this->pushServer->begin();
    this->pushServer->onClient(
        [thisServer](void *s, AsyncClient *c) {
            printf("A new PushClient connected!\n");
            thisServer->registerClient(c, &thisServer->pushClient);
        },
        NULL);
}

void SocketServer::registerClient(AsyncClient *new_client,
                                  AsyncClient **current_client) {
    if (new_client == NULL) {
        debug("a new Client is NULL\n");
        return;
    }

    if (*current_client == nullptr) {
        *current_client = new_client;
    } else {
        debug("Only one socket client allowed.\n");
        new_client->close(true);
        new_client->free();
        delete new_client;
        return;
    }

    void (*handler)(size_t, uint8_t *) = this->handler;
    SocketServer *thisServer = this;
    new_client->onError(
        [thisServer](void *r, AsyncClient *t_client, int8_t error) {
            debug("ClientSocket Error %" PRIu8 "\n", error);
        },
        NULL);
    new_client->onDisconnect(
        [thisServer](void *r, AsyncClient *t_client) {
            debug("Client Disconnected\n");
            thisServer->unregisterClient(t_client);
        },
        NULL);
    new_client->onTimeout(
        [thisServer](void *r, AsyncClient *t_client, uint32_t time) {
            debug("Client timeouted\n");
            thisServer->unregisterClient(t_client);
        },
        NULL);
    new_client->onData([handler](void *r, AsyncClient *t_client, void *buf,
                                 size_t len) { handler(len, (uint8_t *)buf); },
                       NULL);
}

void SocketServer::unregisterClient(AsyncClient *t_client) {
    if (this->pullClient == t_client) {
        this->pullClient = nullptr;
    } else if (this->pushClient == t_client) {
        this->pushClient = nullptr;
    }
    t_client->close(true);  // TODO potential issue: close twice same client
    t_client->free();
    delete t_client;
}

void SocketServer::write2Client(AsyncClient *client, const char *buf,
                                size_t size_buf) {
    if (client == nullptr) return;
    size_t space_left = client->space();
    client->add(buf, size_buf > space_left ? space_left : size_buf);
    client->send();
    if (size_buf <= space_left) return;
    write2Client(client, buf + space_left, size_buf - space_left);
}

void SocketServer::printf2Client(AsyncClient *client, const char *format, ...) {
    va_list args;
    va_start(args, format);

    if (client == nullptr) return;

    uint32_t BUFF_SIZE = 300;
    char buffer[BUFF_SIZE];
    int l = vsnprintf(buffer, BUFF_SIZE, format, args);
    if (l == BUFF_SIZE) FATAL("Buffer not big enough: %d\n", l);
    this->write2Client(client, buffer, l);
    va_end(args);
}

bool SocketServer::hasPushClient() { return this->pushClient != nullptr; }
#endif
