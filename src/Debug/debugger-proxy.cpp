#include "debugger-private.h"

void Debugger::proxify() {
    WARDuino::instance()->program_state = PROXYhalt;
    this->proxy = new Proxy();  // TODO delete
}

void Debugger::handle_proxy_call(Module *m, RunningState *,
                                 uint8_t *interruptData) const {
    if (this->proxy == nullptr) {
        dbg_info("No proxy available to send proxy call to.\n");
        // TODO how to handle this error?
        return;
    }
    uint8_t *data = interruptData;
    uint32_t fidx = read_L32(&data);
    dbg_info("Proxycall func %" PRIu32 "\n", fidx);

    Block *func = &m->functions[fidx];
    StackValue *args = Proxy::readRFCArgs(func, data);
    dbg_trace("Enqueuing callee %" PRIu32 "\n", func->fidx);

    auto *rfc = new RFC(fidx, func->type, args);
    this->proxy->pushRFC(m, rfc);
}

RFC *Debugger::top_proxy_call() const {
    if (proxy == nullptr) {
        return nullptr;
    }
    return this->proxy->topRFC();
}

void Debugger::send_proxy_call_result(Module *m) const {
    if (proxy == nullptr) return;
    RFC *rfc = proxy->returnResult(m);
    if (rfc == nullptr) return;
    debug_RemoteFunctionResult result = debug_RemoteFunctionResult_init_zero;
    result.success = rfc->success;
    send_notification(
        debug_NotificationType_NOTIFICATION_REMOTE_FUNCTION_RESULT,
        debug_RemoteFunctionResult_fields, &result);
    delete rfc;
}

bool Debugger::is_proxy() const { return this->proxy != nullptr; }

bool Debugger::is_proxied(const uint32_t fidx) const {
    return this->supervisor != nullptr && this->supervisor->is_proxied(fidx);
}

void Debugger::handle_monitor_proxies(const Module *m,
                                      uint8_t *interruptData) const {
    const uint32_t amount_funcs = read_B32(&interruptData);
    printf("funcs_total %" PRIu32 "\n", amount_funcs);

    m->warduino->debugger->supervisor->unregisterAllProxiedCalls();
    for (uint32_t i = 0; i < amount_funcs; i++) {
        const uint32_t fidx = read_B32(&interruptData);
        printf("registering fid=%" PRIu32 "\n", fidx);
        m->warduino->debugger->supervisor->registerProxiedCall(fidx);
    }

    debug("done!\n");
}

void Debugger::start_proxy_supervisor(Channel *socket) {
    this->connected_to_proxy = true;
    this->supervisor = new ProxySupervisor(socket, this->supervisor_mutex);
    printf("Connected to proxy.\n");
}

bool Debugger::proxy_connected() const { return this->connected_to_proxy; }

void Debugger::disconnect_proxy() const {
    if (!this->proxy_connected()) {
        return;
    }
    // TODO close file
    this->supervisor_mutex->unlock();
    this->supervisor->thread.join();
}
