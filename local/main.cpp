#include <iostream>
#include "../WARDuino.h"
#include <csignal>

extern "C" {
// TODO: Stat files, alternative needed for arduino
#include <sys/stat.h>
// END
}

WARDuino wac;

volatile bool handelingInterrupt = false;

void signalHandler(int /* signum */) {
    if (handelingInterrupt) return;

    printf("CHANGE REQUESTED!");
    struct stat statbuff{};
    if (stat("/tmp/change", &statbuff) == 0 && statbuff.st_size > 0) {
        uint8_t *data = (uint8_t *)malloc(statbuff.st_size * sizeof(uint8_t));
        FILE *fp = fopen("/tmp/change", "rb");
        fread(data, statbuff.st_size, 1, fp);
        fclose(fp);
        wac.handleInterrupt(statbuff.st_size, data);
    }

    handelingInterrupt = false;
}

#include "../wa_sources/hello_world.c"

/**
 * Run code, ececute interrups in /tmp/change if a USR1 signal comes
*/
int main(int /*argc*/, const char **/*argv*/) {
    signal(SIGUSR1, signalHandler);
    wac.run_module(wac.load_module(hello_world_wasm, hello_world_wasm_len,{}));
    return 0;
}
