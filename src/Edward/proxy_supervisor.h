#pragma once

#include <cinttypes>
#include <csignal>
#include <set>

#include "../Utils/sockets.h"
#include "RFC.h"
#ifndef ARDUINO
#include <nlohmann/json.hpp>
#else
#include "../../lib/json/single_include/nlohmann/json.hpp"
#endif
#include "pthread.h"
#include "sys/types.h"

class ProxySupervisor {
   private:
    Channel *channel;
    pthread_t threadid;
    pthread_mutex_t *mutex;
    std::set<uint32_t> *proxied = new std::set<uint32_t>();

    bool hasReplied = false;
    nlohmann::basic_json<> proxyResult;

    struct SerializeData *serializeRFC(RFC *callee);
    void deserializeRFCResult(RFC *rfc);

   public:
    ProxySupervisor(Channel *duplex, pthread_mutex_t *mutex);

    void listenToSocket();

    bool send(void *t_buffer, int t_size);
    nlohmann::basic_json<> readReply();

    pthread_t getThreadID();

    bool call(RFC *callee);

    void registerProxiedCall(uint32_t fidx);
    void unregisterProxiedCall(uint32_t fidx);
    void unregisterAllProxiedCalls();
    bool isProxied(uint32_t fidx);
};
