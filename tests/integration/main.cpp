#include <fcntl.h>
#include <sys/stat.h>

#include <csignal>
#include <cstdlib>
#include <iostream>

#include "wasm_tests.h"

WARDuino wac;

volatile bool handlingInterrupt = false;

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

/**
 * Run code, execute interrupts in /tmp/change if a USR1 signal comes
 */
int main(int argc, char **argv) {
    signal(SIGUSR1, signalHandler);

    // Load the path name
    // TODO parse args
    // TODO add verbose args
    char *module_file_path = argv[1];
    char *asserts_file_path = argv[2];
    char *wasm_command = argv[3];

    return run_wasm_test(wac, module_file_path, asserts_file_path,
                         wasm_command);
}
