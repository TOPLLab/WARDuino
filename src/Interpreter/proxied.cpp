#include "./proxied.h"

#include "../Debug/debugger.h"
#include "../Utils//util.h"

void send_leb(Channel *channel, uint32_t value, const char *end = "") {
    uint8_t *buffer = write_LEB(value);
    uint32_t size = size_leb(value);
    for (int i = 0; i < size; ++i) {
        channel->write("%" PRIx8 "%s", buffer[i], end);
    }
    free(buffer);
}

bool Proxied::store(Module *m, uint8_t type, uint32_t addr, StackValue &sval) {
    m->warduino->debugger->channel->write("%" PRIx8, interruptStore);
    send_leb(m->warduino->debugger->channel, addr);
    send_leb(m->warduino->debugger->channel, 0);
    send_leb(m->warduino->debugger->channel, sval.value.uint32, "\n");
    return true;
}
