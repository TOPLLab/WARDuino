#include <zephyr/console/console.h>

#include "../../src/WARDuino.h"
#include "upload.h"

#define DEBUGGER_STACK_SIZE 2048
#define DEBUGGER_PRIORITY 0

BUILD_ASSERT(DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_console), zephyr_cdc_acm_uart),
	     "Console device is not ACM CDC UART device");

WARDuino* wac = WARDuino::instance();

void startDebuggerStd() {
    Channel* duplex = new Duplex(stdin, stdout);
    wac->debugger->setChannel(duplex);
    duplex->open();

	console_init();
    int valread;
    //uint8_t buffer[1024] = {0};
	uint8_t buffer[1] = {0};
    while (true) {
		k_msleep(1000);
		
		// TODO: console_read is blocking so we can only read one char at a time
		// otherwise we would need 1024 chars in debugging messages before we
		// process them.
        //while ((valread = duplex->read(buffer, 1024)) != -1) {
		while ((valread = console_read(NULL, buffer, 1)) != -1) {
            wac->handleInterrupt(valread, buffer);
        }
    }
}

K_THREAD_DEFINE(debugger_tid, DEBUGGER_STACK_SIZE,
                startDebuggerStd, NULL, NULL, NULL,
                DEBUGGER_PRIORITY, 0, 0);

int main(void)
{	
    Module* m = wac->load_module(upload_wasm, upload_wasm_len, {});
    wac->run_module(m);
    wac->unload_module(m);
	
	return 0;
}
