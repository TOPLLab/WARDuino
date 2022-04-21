#ifdef ARDUINO
#include "SocketServer.h"

#include "Arduino.h"

#include <WiFi.h>
#include <inttypes.h>
#include <stdio.h>

SocketServer *SocketServer::socketServer = nullptr;

SocketServer::SocketServer(uint16_t t_port, WARDuino *t_wrd) : portno(t_port) {
    asyncServer = nullptr;
    warduino = t_wrd;
}

void SocketServer::initializeServer(uint16_t t_port, WARDuino *t_wrd) {
    if (socketServer == nullptr) socketServer = new SocketServer(t_port, t_wrd);
}

SocketServer *SocketServer::getServer() { return socketServer; }

void SocketServer::connect2Wifi(ServerCredentials *t_credentials) {
    Serial.println("Connecting to WiFi..");
    WiFi.begin(t_credentials->ssid, t_credentials->pswd);
    while (WiFi.status() != WL_CONNECTED) {
        delay(10);
    }

    Serial.printf("%d.%d.%d.%d\n\n", WiFi.localIP()[0], WiFi.localIP()[1],
                  WiFi.localIP()[2], WiFi.localIP()[3]);
    printf("Connected and this is %p\n", this);
}

void SocketServer::begin() {
    printf("prior new AsyncServer()\n");
    printf("%p\n", this);
    if (this == nullptr) {
        printf("yes nullptr!!!\n");
    }
    printf("portno %d\n", this->portno);

    this->asyncServer = new AsyncServer(this->portno);
    printf("this->asyncServer->begin()\n");
    this->asyncServer->begin();
    printf("this->asyncServer->onClient()\n");
    this->asyncServer->onClient(
        [this](void *s, AsyncClient *c) {
            printf("A client connected!\n");
            this->registerClient(c);
        },
        NULL);
    printf("done with SocketServer::begin()\n");
}

void SocketServer::registerClient(AsyncClient *t_client) {
    if (t_client == NULL) {
        printf("Client is NULL\n");
        return;
    }

    if (this->client == nullptr) {
        this->client = t_client;
    } else {
        printf("Only one socket client allowed.\n");
        t_client->close(true);
        t_client->free();
        delete t_client;
        return;
    }

    WARDuino *wrd = this->warduino;
    SocketServer *thisServer = this;
    t_client->onError(
        [thisServer](void *r, AsyncClient *t_client, int8_t error) {
            printf("ClientSocket Error %" PRIu8 "\n", error);
        },
        NULL);
    t_client->onDisconnect(
        [thisServer](void *r, AsyncClient *t_client) {
            printf("Client Disconnected\n");
            thisServer->unregisterClient(t_client);
        },
        NULL);
    t_client->onTimeout(
        [thisServer](void *r, AsyncClient *t_client, uint32_t time) {
            printf("Client timeouted\n");
            thisServer->unregisterClient(t_client);
        },
        NULL);
    t_client->onData(
        [wrd](void *r, AsyncClient *t_client, void *buf, size_t len) {
            wrd->handleInterrupt(len, (uint8_t *)buf);
        },
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
    // const char* cbuf = (char*)buf;
    // size_t size_buf = (size_t) count;
    size_t space_left = client->space();
    client->add(buf, size_buf > space_left ? space_left : size_buf);
    client->send();
    if (size_buf <= space_left) return;
    write2Client(buf + space_left, size_buf - space_left);
}
#endif ARDUINO
