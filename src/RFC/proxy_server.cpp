#include "proxy_server.h"
/* #include <asm-generic/errno-base.h> */  // Might be needed
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>

#include "../Utils/util.h"

// TODO exceptionmsg
const char NO_HOST_ERR[] = "No host and port set";
const char CREATE_SOCK_ERR[] = "Could not create Socket";
const char INVALID_HOST[] = "Invalid host";
const char CONNECT_ERR[] = "Socket failed to connect";
const char WRITE_ERR[] = "ERROR writing to socket";
const char READ_ERR[] = "ERROR reading from socket";

struct ProxyServer::Address {
    struct sockaddr_in aserv_addr;
    struct hostent *aServer;
};

ProxyServer *ProxyServer::proxyServer = nullptr;

ProxyServer *ProxyServer::getServer() {
    if (proxyServer == nullptr) proxyServer = new ProxyServer();
    return proxyServer;
}

void ProxyServer::registerMCUHost(uint8_t **data) {
    int portno = (int)read_B32(data);
    uint8_t hostsize = (uint8_t)(*data)[0];
    char *hostname = new char[hostsize + 1];
    memcpy((void *)hostname, ++(*data), hostsize);
    hostname[hostsize] = '\0';
    printf("Registering Proxy Host: %s PORT=%d\n", hostname, portno);
    ProxyServer::getServer()->registerAdress(hostname, portno);
}

void ProxyServer::registerAdress(char *t_host, int t_port) {
    if (this->host != nullptr) {
        this->closeConnection();
        free(this->host);
    }
    this->host = t_host;
    this->port = t_port;
}

void ProxyServer::updateExcpMsg(const char *msg) {
    if (this->exceptionMsg != nullptr) delete[] this->exceptionMsg;
    auto msg_len = strlen(msg);
    this->exceptionMsg = new char[(msg_len + 1) / sizeof(char)];
    this->exceptionMsg[msg_len] = '\0';
    memcpy(this->exceptionMsg, msg, msg_len);
}

ProxyServer::ProxyServer() {
    host = exceptionMsg = nullptr;
    port = 0;
    sockfd = -1;
    address = (struct Address *)malloc(sizeof(struct Address));
}

bool ProxyServer::openConnection() {
    if (this->host == nullptr) {
        this->updateExcpMsg(NO_HOST_ERR);
        return false;
    }

    this->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (this->sockfd < 0) {
        this->updateExcpMsg(CREATE_SOCK_ERR);
        return false;
    }

    struct hostent *aServer = gethostbyname(this->host);
    if (aServer == NULL) {
        this->updateExcpMsg(INVALID_HOST);
        return false;
    }

    this->address->aServer = aServer;
    struct sockaddr_in *aserv_addr = &this->address->aserv_addr;
    bzero((char *)aserv_addr, sizeof(*aserv_addr));
    aserv_addr->sin_family = AF_INET;
    bcopy((char *)aServer->h_addr, (char *)&aserv_addr->sin_addr.s_addr,
          aServer->h_length);
    aserv_addr->sin_port = htons(this->port);
    if (connect(sockfd, (struct sockaddr *)aserv_addr, sizeof(*aserv_addr)) <
        0) {
        this->updateExcpMsg(CONNECT_ERR);
        return false;
    }
    return true;
}

void ProxyServer::closeConnection() {
    if (this->sockfd != -1) {
        if (close(this->sockfd) == -1) {
            if (errno == EINTR) close(this->sockfd);
        }
        this->sockfd = -1;
    }
}

bool ProxyServer::send(void *buffer, int size) {
    int n = write(this->sockfd, buffer, size);
    if (n == size) return true;

    if (n < 0 && errno == EINTR)  // write interrupted, thus retry
        return this->send(buffer, size);
    else if (n < 0) {
        this->updateExcpMsg(WRITE_ERR);
        return false;
    }
    // send remaining bytes
    char *buf = (char *)buffer + n;
    return this->send((void *)buf, size - n);
}

char *ProxyServer::readReply(short int amount) {
    char *buffer = new char[amount + 1];
    bzero(buffer, amount + 1);
    int n = read(this->sockfd, buffer, amount);
    if (n > 0) return buffer;

    delete[] buffer;
    if (errno == EINTR)  // read interrupted, thus retry
        return this->readReply(amount);

    this->updateExcpMsg(READ_ERR);
    return nullptr;
}
