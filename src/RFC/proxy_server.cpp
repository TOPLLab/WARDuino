#include "proxy_server.h"
/* #include <asm-generic/errno-base.h> */  // Might be needed
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../Utils/util.h"

// TODO exception msg
const char SUCCESS[] = "";
const char NO_HOST_ERR[] = "No host and port set";
const char CREATE_SOCK_ERR[] = "Could not create Socket";
const char INVALID_HOST[] = "Invalid host";
const char CONNECT_ERR[] = "Socket failed to connect";
const char WRITE_ERR[] = "ERROR writing to socket";
const char READ_ERR[] = "ERROR reading from socket";

struct Address {
    struct sockaddr_in aserv_addr;
    struct hostent *aServer;
};

const char *createConnection(int socketfd, char *host, int port,
                             struct Address *address) {
    struct hostent *server = gethostbyname(host);
    if (server == nullptr) {
        return INVALID_HOST;
    }

    address->aServer = server;
    struct sockaddr_in *server_address = &address->aserv_addr;
    bzero((char *)server_address, sizeof(*server_address));
    server_address->sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&server_address->sin_addr.s_addr,
          server->h_length);
    server_address->sin_port = htons(port);
    if (connect(socketfd, (struct sockaddr *)server_address,
                sizeof(*server_address)) < 0) {
        return CONNECT_ERR;
    }

    return SUCCESS;
}

bool is_success(const char *msg) {
    return (msg != nullptr) && (msg[0] == '\0');
}

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
    printf("connecting");
    if (this->host == nullptr) {
        this->updateExcpMsg(NO_HOST_ERR);
        return false;
    }

    this->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (this->sockfd < 0) {
        this->updateExcpMsg(CREATE_SOCK_ERR);
        return false;
    }

    const char *msg =
        createConnection(sockfd, this->host, this->port, this->address);
    if (!is_success(msg)) {
        this->updateExcpMsg(msg);
        return false;
    }
    printf("connected");
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
