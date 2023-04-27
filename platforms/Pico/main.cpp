#include <stdio.h>

#include "../../src/WARDuino.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "upload.h"

unsigned int wasm_len = upload_wasm_len;
unsigned char* wasm = upload_wasm;

WARDuino *wac;

/*void startDebuggerStd() {
    Channel* duplex = new Duplex(stdin, stdout);
    wac->debugger->setChannel(duplex);
    duplex->open();

    int valread;
    uint8_t buffer[1024] = {0};
    while (true) {
        sleep_ms(1000);

        while ((valread = duplex->read(buffer, 1024)) != -1) {
            wac->handleInterrupt(valread - 1, buffer);
        }
    }
}*/

int main() {
    stdio_init_all();
    
    printf("Warduino hello!\n");
    
    WARDuino* wac = WARDuino::instance();
    Module* m = wac->load_module(wasm, wasm_len, {});
   //multicore_launch_core1(startDebuggerStd);
    
    wac->run_module(m);
    wac->unload_module(m);
}
