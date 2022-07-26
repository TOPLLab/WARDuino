#pragma once

#include <cinttypes>
#include <csignal>

#include "../Utils/sockets.h"
#include "../WARDuino.h"
#include "RFC.h"
#include "pthread.h"
#include "sys/types.h"

class ProxySupervisor {
   private:
    Channel *channel;
    pthread_t threadid;
    pthread_mutex_t *mutex;
    std::map<uint32_t, RFC *> functions;

    struct SerializeData *serializeRFC(RFC *callee);
    void deserializeRFCResult(RFC *rfc);
   public:
    ProxySupervisor(int socket, pthread_mutex_t *mutex);

    void startPushDebuggerSocket();

    bool send(void *t_buffer, int t_size);
    char *readReply(short int amount = 1024);

    pthread_t getThreadID();

    bool call(RFC *callee);
};
