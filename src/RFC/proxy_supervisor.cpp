#ifndef ARDUINO
#include "proxy_supervisor.h"

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <nlohmann/json.hpp>

#include "../Utils/macros.h"
#include "../Utils/sockets.h"
#include "../Utils/util.h"

// TODO exception msg
const char SUCCESS[] = "";  // Empty denotes success
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

bool is_success(const char *msg) {
    return (msg != nullptr) && (msg[0] == '\0');  // check if string is empty
}

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

bool continuing(pthread_mutex_t *mutex) {
    switch (pthread_mutex_trylock(mutex)) {
        case 0: /* if we got the lock, unlock and return false */
            pthread_mutex_unlock(mutex);
            return false;
        case EBUSY: /* return true if the mutex was locked */
        default:
            return true;
    }
}

void *readSocket(void *input) {
    // Print value received as argument:
    dbg_info("\n=== LISTENING TO SOCKET (in separate thread) ===\n");
    auto *supervisor = (ProxySupervisor *)input;
    supervisor->startPushDebuggerSocket();
    pthread_exit(nullptr);
}

Event *parseJSON(char *buff) {
    // TODO duplicate code in Debugger::handlePushedEvent
    nlohmann::basic_json<> parsed = nlohmann::json::parse(buff);
    printf("parseJSON: %s\n", parsed.dump().c_str());
    std::string payload = *parsed.find("payload");
    return new Event(*parsed.find("topic"), payload);
}

void ProxySupervisor::updateExcpMsg(const char *msg) {
    delete[] this->exceptionMsg;
    auto msg_len = strlen(msg);
    this->exceptionMsg = new char[(msg_len + 1) / sizeof(char)];
    this->exceptionMsg[msg_len] = '\0';
    memcpy(this->exceptionMsg, msg, msg_len);
}

ProxySupervisor::ProxySupervisor(int socket, pthread_mutex_t *mutex) {
    printf("Started supervisor.\n");
    this->socket = socket;
    this->channel = new Channel(socket);
    this->mutex = mutex;

    pthread_create(&this->threadid, nullptr, readSocket, this);
}

void ProxySupervisor::startPushDebuggerSocket() {
    char _char;
    uint32_t buf_idx = 0;
    const uint32_t start_size = 1024;
    uint32_t current_size = start_size;
    char *buffer = (char *)malloc(start_size);

    printf("Started listening for events from proxy device.\n");
    while (continuing(this->mutex)) {
        if (read(this->socket, &_char, 1) != -1) {
            // increase buffer size if needed
            if (current_size <= (buf_idx + 1)) {
                char *new_buff = (char *)malloc(current_size + start_size);
                memcpy((void *)new_buff, (void *)buffer, current_size);
                free(buffer);
                buffer = new_buff;
                current_size += start_size;
                printf("increasing PushClient's buffer size to %d\n",
                       current_size);
            }
            buffer[buf_idx++] = _char;
            // manual null-termination is needed because parseJSON does not use
            // first len argument
            buffer[buf_idx] = '\0';
            try {
                Event *event = parseJSON(buffer);
                CallbackHandler::push_event(event);
                WARDuino::instance()->debugger->notifyPushedEvent();
                buf_idx = 0;
            } catch (const nlohmann::detail::parse_error &e) {
            }
        }
    }
}

bool ProxySupervisor::send(void *buffer, int size) {
    int n = write(this->socket, buffer, size);
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

char *ProxySupervisor::readReply(short int amount) {
    char *buffer = new char[amount + 1];
    bzero(buffer, amount + 1);
    int n = read(this->socket, buffer, amount);
    if (n > 0) return buffer;

    delete[] buffer;
    if (errno == EINTR)  // read interrupted, thus retry
        return this->readReply(amount);

    this->updateExcpMsg(READ_ERR);
    return nullptr;
}
pthread_t ProxySupervisor::getThreadID() { return this->threadid; }
#endif
