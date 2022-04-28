#include "CallbackHandler.h"

#include <algorithm>

#include "../Debug/debugger.h"
#include "../Interpreter/instructions.h"
#ifdef ARDUINO
#include "../RFC/SocketServer.h"
#endif
#include "../Utils/macros.h"

// CallbackHandler class

bool CallbackHandler::pushingMode = false;

// bool CallbackHandler::resolving_event = false;
std::unordered_map<std::string, std::vector<Callback> *>
    *CallbackHandler::callbacks =
        new std::unordered_map<std::string, std::vector<Callback> *>();
std::queue<Event> *CallbackHandler::events = new std::queue<Event>();

void CallbackHandler::add_callback(const Callback &c) {
    auto item = callbacks->find(c.topic);
    if (item == callbacks->end()) {
        auto *list = new std::vector<Callback>();
        list->push_back(c);
        callbacks->emplace(c.topic, list);
    } else {
        item->second->push_back(c);
    }
}

void CallbackHandler::remove_callback(const Callback &c) {
    // Remove callbacks with the same table_index as c from the list of
    // callbacks for the topic of c.
    auto list = callbacks->find(c.topic)->second;
    list->erase(std::remove_if(list->begin(), list->end(),
                               [c](Callback const &cb) {
                                   return c.table_index == cb.table_index;
                               }),
                list->end());
}

void CallbackHandler::push_event(std::string topic,
                                 const unsigned char *payload,
                                 unsigned int length) {
    if (events->size() < EVENTS_SIZE) {
        char *message = (char *)(malloc(sizeof(char) * length + 1));
        snprintf(message, length + 1, "%s", payload);
        auto e = new Event(std::move(topic),
                           reinterpret_cast<const char *>(message));
        dbg_info("Push Event(%s, %s)\n", e->topic.c_str(), e->payload);
        events->push(*e);
    }
}

bool CallbackHandler::resolve_event() {
    if (CallbackHandler::events->empty()) {
        return false;
    }
    //    CallbackHandler::resolving_event = true;

    Event event = CallbackHandler::events->front();

#ifdef ARDUINO
    // check if we need to push events
    SocketServer *server = SocketServer::getServer();
    if (CallbackHandler::pushingMode && !server->hasPushClient()) {
        printf("pushingMode activated but client to push to\n");
        return true;
    }
#endif
    CallbackHandler::events->pop();

    printf("Resolving an event. (%lu remaining)\n",
           CallbackHandler::events->size());

    auto iterator = CallbackHandler::callbacks->find(event.topic);
    if (iterator != CallbackHandler::callbacks->end()) {
        std::string key = iterator->first;
        for (Callback cbs : *iterator->second) {
            cbs.resolve_event(event);
        }
    } else {
        // TODO handle error: event for non-existing iterator
    }
    //    CallbackHandler::resolving_event = false;
    return !CallbackHandler::events->empty();
}

// Callback class

Callback::Callback(Module *m, std::string id, uint32_t tidx) {
    this->module = m;
    this->topic = std::move(id);
    this->table_index = tidx;
}

void Callback::resolve_event(const Event &e) {
    dbg_trace("Callback(%s, %i): resolving Event(%s, \"%s\")\n", topic.c_str(),
              table_index, e.topic.c_str(), e.payload);

#ifdef ARDUINO
    if (CallbackHandler::pushingMode) {
        SocketServer *server = SocketServer::getServer();
        printf(R"({"topic":"%s","payload":"%s"})", e.topic.c_str(),
               e.payload);  // TODO remove
        server->printf2Client(server->pushClient,
                              R"({"topic":"%s","payload":"%s"})",
                              e.topic.c_str(), e.payload);
        return;
    }
#endif

    // Copy topic and payload to linear memory
    uint32_t start = 10000;  // TODO use reserved area in linear memory
    std::string topic = e.topic;
    std::string payload = e.payload;
    for (unsigned long i = 0; i < topic.length(); i++) {
        module->memory.bytes[start + i] = (uint32_t)e.topic[i];
    }
    start += topic.length();
    for (unsigned long i = 0; i < payload.length(); i++) {
        module->memory.bytes[start + i] = (uint32_t)e.payload[i];
    }

    // Push arguments (5 args)
    module->stack[++module->sp].value.uint32 = start - topic.length();
    module->stack[++module->sp].value.uint32 = topic.length();
    module->stack[++module->sp].value.uint32 = start;
    module->stack[++module->sp].value.uint32 = payload.length();
    module->stack[++module->sp].value.uint32 = payload.length();

    // Setup function
    uint32_t fidx = module->table.entries[table_index];
    setup_call(module, fidx);

    // Validate argument count
    // Callback function cannot return a result, should have return type void
    // TODO
}

Callback::Callback(const Callback &c) {
    module = c.module;
    topic = c.topic;
    table_index = c.table_index;
}

// Event class

Event::Event(std::string topic, const char *payload) {
    this->topic = topic;
    this->payload = payload;
}
