#include "./wasm.h"
#include "../Debug/debugger.h"

bool Proxied::store(Module *m, uint8_t type, uint32_t addr, StackValue &sval) {
    // TODO send back to debugger
    uint8_t *buf = calloc(sizeof(uint8_t), 20);
    uint32_t size = write_LEB_32(addr, *buf, 20);
    uint8_t *msg = calloc(sizeof(uint8_t), size + 3);
	msg[0] = interruptStore;
	msg[1] = 0;
	memcpy(msg + 2, sval.value, 4);
	msg[19] = '\0';
    m->warduino->debugger->channel->write(msg);
}
