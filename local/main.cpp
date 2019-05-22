#include <iostream>
#include "../WARDuino.h"
#include "signal.h"

extern "C" {
// TODO: Stat files, alternative needed for arduino
#include <sys/stat.h>
// END
}

WARDuino wac;

volatile bool handelingInterput = false;

void signalHandler(int signum) {
    if (handelingInterput) return;

    printf("CHANGE REQUESTED!");
    struct stat statbuff;
    if (stat("/tmp/change", &statbuff) == 0 && statbuff.st_size > 0) {
        uint8_t *data = (uint8_t *)malloc(statbuff.st_size * sizeof(uint8_t));
        FILE *fp = fopen("/tmp/change", "rb");
        fread(data, statbuff.st_size, 1, fp);
        fclose(fp);
        wac.handleInterupt(statbuff.st_size, data);
    }

    handelingInterput = false;
}

#include "../wa_sources/hello_world.c"

/**
 * Run code, ececute interups in /tmp/change if a USR1 signal comes
*/
int main(int argc, const char *argv[]) {
    signal(SIGUSR1, signalHandler);
    wac.run_module(hello_world_wasm, hello_world_wasm_len);
    return 0;
}
