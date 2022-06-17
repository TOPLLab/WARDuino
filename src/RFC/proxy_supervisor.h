#pragma once

#include <cinttypes>
#include <csignal>

#include "../Utils/sockets.h"
#include "pthread.h"
#include "sys/types.h"

struct Address;

class ProxySupervisor {
   private:
    Channel *channel;
    int socket;
    pthread_t threadid;
    pthread_mutex_t *mutex;

   public:
    char *exceptionMsg;

    // private constructor for singleton
    ProxySupervisor(int socket, pthread_mutex_t *mutex);

    void startPushDebuggerSocket();

    void updateExcpMsg(const char *msg);
    bool send(void *t_buffer, int t_size);
    char *readReply(short int amount = 1024);

    pthread_t getThreadID();
};
