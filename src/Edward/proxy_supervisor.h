#pragma once

#include <cinttypes>
#include <csignal>
#include <set>

#include "../Utils/sockets.h"
#include "RFC.h"
#include "pthread.h"
#include "sys/types.h"

class ProxySupervisor {
   private:
    Channel *channel;
    pthread_t threadid;
    pthread_mutex_t *mutex;
    std::set<uint32_t> *proxied = new std::set<uint32_t>();

    struct SerializeData *serializeRFC(RFC *callee);
    void deserializeRFCResult(RFC *rfc);

   public:
    ProxySupervisor(Channel *duplex, pthread_mutex_t *mutex);

    void startPushDebuggerSocket();

    bool send(void *t_buffer, int t_size);
    char *readReply(short int amount = 1024);

    pthread_t getThreadID();

    bool call(RFC *callee);

    void registerProxiedCall(uint32_t fidx);
    void unregisterProxiedCall(uint32_t fidx);
    void unregisterAllProxiedCalls();
    bool isProxied(uint32_t fidx);
};
