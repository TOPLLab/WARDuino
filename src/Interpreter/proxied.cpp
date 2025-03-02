#include "./proxied.h"

#include "../Debug/debugger.h"
#include "../Utils//util.h"

void send_leb(Channel *channel, uint32_t value, const char *end = "") {
    uint8_t *buffer = write_LEB(value);
    uint32_t size = size_leb(value);
    for (uint32_t i = 0; i < size; ++i) {
        channel->write("%02" PRIx8 "%s", buffer[i], end);
    }
    free(buffer);
}

bool Proxied::store(Module *m, [[maybe_unused]] uint8_t type, uint32_t addr,
                    StackValue &sval) {
    Interpreter::store(m, type, addr, sval);
//    m->warduino->debugger->channel->write("%02" PRIx8, interruptStore);
//    send_leb(m->warduino->debugger->channel, addr);
//    send_leb(m->warduino->debugger->channel, 0);
//    send_leb(m->warduino->debugger->channel, sval.value.uint32, "\n");
    return true;
}
