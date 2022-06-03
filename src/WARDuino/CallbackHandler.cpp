#include "CallbackHandler.h"

#include <algorithm>

#include "../Debug/debugger.h"
#include "../Interpreter/instructions.h"
#ifdef ARDUINO
#include "../RFC/SocketServer.h"
#endif
#include "../Utils/macros.h"

// CallbackHandler class

bool CallbackHandler::manual_event_resolution = false;
bool CallbackHandler::resolving_event = false;
#ifdef ARDUINO
size_t CallbackHandler::pushed_cursor = 0;

bool should_push_event(SocketServer *server) {  // TODO replace with WARDuinoDrone RunningState check
    return server != nullptr && server->hasPushClient() &&
           CallbackHandler::pushed_cursor < CallbackHandler::event_count();
}
#endif

std::unordered_map<std::string, std::vector<Callback> *>
    *CallbackHandler::callbacks =
        new std::unordered_map<std::string, std::vector<Callback> *>();
std::deque<Event> *CallbackHandler::events = new std::deque<Event>();

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

size_t CallbackHandler::callback_count(const std::string &topic) {
    return callbacks->find(topic)->second->size();
}

// WARNING: Push event functions should not use IO functions, since they can be
// called from ISR callbacks
void CallbackHandler::push_event(std::string topic,
                                 const unsigned char *payload,
                                 unsigned int length) {
    char *message = (char *)(malloc(sizeof(char) * length + 1));
    snprintf(message, length + 1, "%s", payload);
    auto event =
        new Event(std::move(topic), reinterpret_cast<const char *>(message));
    CallbackHandler::push_event(event);
}

void CallbackHandler::push_event(Event *event) {
    if (events->size() < EVENTS_SIZE) {
        events->push_back(*event);
    }
}

bool CallbackHandler::resolve_event(bool force) {
    if (CallbackHandler::resolving_event || CallbackHandler::events->empty()) {
        return false;
    }
    Event event = CallbackHandler::events->front();

#ifdef ARDUINO
    SocketServer *server = SocketServer::getServer();
    if (should_push_event(server)) {
        Event e = CallbackHandler::events->at(CallbackHandler::pushed_cursor++);
        server->printf2Client(server->pushClient,
                              R"({"topic":"%s","payload":"%s"})",
                              e.topic.c_str(), e.payload.c_str());
    }
#endif

    if (!force && (CallbackHandler::manual_event_resolution ||
                   WARDuino::instance()->program_state == WARDUINOpause)) {
        return true;
    }

    CallbackHandler::resolving_event = true;
    CallbackHandler::events->pop_front();
#ifdef ARDUINO
    CallbackHandler::pushed_cursor--;
#endif

    debug("Resolving an event. (%lu remaining)\n",
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
    CallbackHandler::resolving_event = false;
    return !CallbackHandler::events->empty();
}

size_t CallbackHandler::event_count() {
    return CallbackHandler::events->size();
}

std::deque<Event>::const_iterator CallbackHandler::event_begin() {
    return CallbackHandler::events->cbegin();
}

std::deque<Event>::const_iterator CallbackHandler::event_end() {
    return CallbackHandler::events->cend();
}

void CallbackHandler::clear_callbacks() { CallbackHandler::callbacks->clear(); }

std::string CallbackHandler::dump_callbacks() {
    std::string repr = R"({"callbacks": [)";
    auto iterator = CallbackHandler::callbacks->begin();
    while (iterator != CallbackHandler::callbacks->end()) {
        repr += R"({")" + iterator->first + R"(": [)";
        for (const auto &value : *iterator->second) {
            auto callback = std::begin(*iterator->second);
            while (callback != std::end(*iterator->second)) {
                repr += std::to_string(callback->table_index);
                repr += (++callback != iterator->second->end()) ? ", " : "";
            }
        }
        iterator++;
        repr += "]}";
        repr += (iterator != CallbackHandler::callbacks->end()) ? ", " : "";
    }
    repr += "]}";
    return repr;
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

Event::Event(std::string topic, std::string payload) {
    this->topic = topic;
    this->payload = payload;
}

std::string Event::serialized() const {
    return R"({"topic": ")" + this->topic + R"(", "payload": ")" +
           this->payload + R"("})";
}
