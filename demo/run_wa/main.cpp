#include <csignal>
#include <iostream>

#include "../../WARDuino.h"

#ifndef SOCKET
#define SOCKET 0
#endif

#ifndef MT
#define MT 0  // multi thread
#endif

#if SOCKET
#include "../../socket_server.h"
#if MT
#include <pthread.h>
#endif
#else
extern "C" {
// TODO: Stat files, alternative needed for arduino
#include <sys/stat.h>
// END
}
#endif

WARDuino wac;

volatile bool handlingInterrupt = false;

#if SOCKET
#if MT
void *listenLoop(void *arg) {
  while (1) {
    processIncomingEvents();
    if (receivedDataSize() > 0) {
      auto *data = (uint8_t *)getReceivedData();
      wac.handleInterrupt(receivedDataSize(), data);
      freeReceivedData();
    }
  }
  pthread_exit(NULL);
  return nullptr;
}
#else
void signalHandler(int /* signum */) {
  if (handlingInterrupt) return;

  processIncomingEvents();
  if (receivedDataSize() > 0) {
    auto *data = (uint8_t *)getReceivedData();
    wac.handleInterrupt(receivedDataSize(), data);
    freeReceivedData();
  }

  handlingInterrupt = false;
}
#endif

#else //No Sockets
void signalHandler(int /* signum */) {
  if (handlingInterrupt) return;

  printf("CHANGE REQUESTED!");
  struct stat statbuff {};
  if (stat("/tmp/change", &statbuff) == 0 && statbuff.st_size > 0) {
    auto *data = (uint8_t *)malloc(statbuff.st_size * sizeof(uint8_t));
    FILE *fp = fopen("/tmp/change", "rb");
    fread(data, statbuff.st_size, 1, fp);
    fclose(fp);
    wac.handleInterrupt(statbuff.st_size, data);
  }

  handlingInterrupt = false;
}
#endif

#include "wa_sources/hello_world.c"

#if SOCKET

int main(int /*argc*/, const char ** /*argv*/) {
    int portno = 8080;
    const char *host = "localhost";
    initializeServer(host, portno);

#if MT
    pthread_t server;
    if (pthread_create(&server, NULL, listenLoop, NULL)) {
        perror("error starting server thread\n");
        exit(-1);
    }
    #else

  signal(SIGUSR1, signalHandler);
  #endif

    wac.run_module(wac.load_module(hello_world_wasm, hello_world_wasm_len,
    {})); return 0;
}
#else
/**
 * Run code, ececute interrups in /tmp/change if a USR1 signal comes
 */

int main(int /*argc*/, const char ** /*argv*/) {
  signal(SIGUSR1, signalHandler);
  wac.run_module(wac.load_module(hello_world_wasm, hello_world_wasm_len, {}));
  return 0;
}
#endif