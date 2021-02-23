#include <iostream>
#include "../../WARDuino.h"
#include <csignal>

extern "C" {
// TODO: Stat files, alternative needed for arduino
#include <sys/stat.h>
// END
}

WARDuino wac;

volatile bool handlingInterrupt = false;

void signalHandler(int /* signum */) {
    if (handlingInterrupt) return;

    printf("CHANGE REQUESTED!");
    struct stat statbuff{};
    if (stat("/tmp/change", &statbuff) == 0 && statbuff.st_size > 0) {
        auto *data = (uint8_t *) malloc(statbuff.st_size * sizeof(uint8_t));
        FILE *fp = fopen("/tmp/change", "rb");
        fread(data, statbuff.st_size, 1, fp);
        fclose(fp);
        wac.handleInterrupt(statbuff.st_size, data);
    }

    handlingInterrupt = false;
}

#include "wa_sources/sensor.c"

/**
 * Run code, execute interrupts in /tmp/change if a USR1 signal comes
*/
int main(int /*argc*/, const char **/*argv*/) {
    signal(SIGUSR1, signalHandler);
    Module *m = wac.load_module(app_wasm, app_wasm_len, {});
    wac.run_module(m);
    wac.unload_module(m);
    return 0;
}
