#pragma once

#include <cinttypes>
//#include <csignal>
#include <mutex>
#include <set>
#include <thread>

#include "../Threading/warduino-thread.h"
#include "../Utils/sockets.h"
#include "RFC.h"
#ifndef ARDUINO
#include <nlohmann/json.hpp>
#else
#include "../../lib/json/single_include/nlohmann/json.hpp"
#endif
#include "sys/types.h"

class ProxySupervisor {
   private:
    Channel *channel;
    warduino::mutex *mutex;
    std::set<uint32_t> *proxied = new std::set<uint32_t>();

    bool hasReplied = false;
    nlohmann::basic_json<> proxyResult;

    struct SerializeData *serializeRFC(RFC *callee);
    void deserializeRFCResult(RFC *rfc);

   public:
    warduino::thread thread;

    ProxySupervisor(Channel *duplex, warduino::mutex *mutex);

    void listenToSocket();

    bool send(void *t_buffer, int t_size);
    nlohmann::basic_json<> readReply();

    bool call(RFC *callee);

    void registerProxiedCall(uint32_t fidx);
    void unregisterProxiedCall(uint32_t fidx);
    void unregisterAllProxiedCalls();
    bool isProxied(uint32_t fidx);
};
