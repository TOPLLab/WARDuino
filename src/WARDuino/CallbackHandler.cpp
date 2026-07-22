#include "CallbackHandler.h"

#include <algorithm>

#include "../Debug/debugger.h"
#include "../Interpreter/instructions.h"
#include "../Utils/macros.h"

void push_guard(Module *m) {
    if (m == nullptr) {
        return;
    }
    auto *guard = (Block *)malloc(sizeof(struct Block));
    guard->block_type = 255;
    guard->type = nullptr;
    guard->local_value_type = nullptr;
    guard->start_ptr = nullptr;
    guard->end_ptr = nullptr;
    guard->else_ptr = nullptr;
    guard->export_name = nullptr;
    guard->import_field = nullptr;
    guard->import_module = nullptr;
    guard->func_ptr = nullptr;
    push_block(m, guard, m->sp);
}

// CallbackHandler class

bool CallbackHandler::manual_event_resolution = false;
bool CallbackHandler::resolving_event = false;
size_t CallbackHandler::pushed_cursor = 0;
bool CallbackHandler::pendingEventsActivated = false;
bool CallbackHandler::callbackEnabled = true;
std::deque<Event *> *CallbackHandler::pendingEvents = new std::deque<Event *>();

bool should_push_event() {
    return WARDuino::instance()->program_state == PROXYrun ||
           WARDuino::instance()->program_state == PROXYhalt;
}

std::unordered_map<std::string, std::vector<Callback> *>
    *CallbackHandler::callbacks =
        new std::unordered_map<std::string, std::vector<Callback> *>();
std::deque<Event> *CallbackHandler::events = new std::deque<Event>();

InstrumentationManager *CallbackHandler::manager = nullptr;

void CallbackHandler::setInstrumentationMangager(
    InstrumentationManager *manager) {
    CallbackHandler::manager = manager;
}

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
void CallbackHandler::push_event(std::string topic, const char *payload,
                                 unsigned int length) {
    if (CallbackHandler::callbackEnabled) {
        char *message = (char *)(malloc(sizeof(char) * length + 1));
        snprintf(message, length + 1, "%s", payload);
        auto event = new Event(std::move(topic),
                               reinterpret_cast<const char *>(message));

        if (CallbackHandler::pendingEventsActivated) {
            if (events->size() + pendingEvents->size() < EVENTS_SIZE) {
                CallbackHandler::pendingEvents->push_back(event);
                WARDuino::instance()->debugger->freshMessages = true;
            }
        } else {
            CallbackHandler::push_event(event);
        }
    }
}

void CallbackHandler::push_event(Event *event) {
    // WARNING: called within an ISR so do not use IO functions!
    if (CallbackHandler::callbackEnabled) {
        if (events->size() + pendingEvents->size() < EVENTS_SIZE) {
            events->push_back(*event);
            WARDuino::instance()->debugger->freshMessages = true;
        }
    }
}

bool CallbackHandler::resolve_event(const Channel &output, Module *module,
                                    bool force) {
    if (CallbackHandler::resolving_event || CallbackHandler::events->empty() ||
        WARDuino::instance()->program_state == WARDUINOpause) {
        return false;
    }

    while (
        CallbackHandler::manager->interceptEvents &&
        CallbackHandler::manager->runHookForOnEventHandling(output, module)) {
    }

    // empty events case is possible since hook can remove events
    if (CallbackHandler::events->empty()) {
        return false;
    }

    // Note front event may be one just pushed via another thread that did not
    // get handled via hooks we need a good mechanism to be thread safe.
    // Maybe use the pending queue as the queue for new events and at each
    // resolve_event the pending queue gets moved to the events queue. If you
    // pendingbuffer then also move the code that run hooks on the pendingbuffer
    // from instructions to here.
    Event event = CallbackHandler::events->front();
    CallbackHandler::resolving_event = true;
    CallbackHandler::events->pop_front();

    debug("Resolving an event. (%zu remaining)\n",
          CallbackHandler::events->size());

    bool resolving_new_event = false;
    auto iterator = CallbackHandler::callbacks->find(event.topic);
    if (iterator != CallbackHandler::callbacks->end()) {
        Module *module = nullptr;
        std::string key = iterator->first;
        for (Callback cbs : *iterator->second) {
            cbs.resolve_event(event);
            module = cbs.module;
            resolving_new_event = true;
        }
        push_guard(module);
    } else {
        // TODO handle error: event for non-existing iterator
        printf("No handler found for %s (in %zu items)!\n", event.topic.c_str(),
               CallbackHandler::callbacks->size());
    }
    return resolving_new_event;
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
        auto callback = std::begin(*iterator->second);
        while (callback != std::end(*iterator->second)) {
            repr += std::to_string(callback->table_index);
            repr += (++callback != iterator->second->end()) ? ", " : "";
        }
        repr += "]}";
        repr += (++iterator != CallbackHandler::callbacks->end()) ? ", " : "";
    }
    repr += "]}";
    return repr;
}

std::string CallbackHandler::dump_callbacksV2(bool includeOuterCurlyBraces) {
    // prints json as {"ballbackid": "interrupt_10", "tableIndexes": [ 0, 2]}
    std::string repr =
        includeOuterCurlyBraces ? R"({"callbacks": [)" : R"("callbacks": [)";
    auto iterator = CallbackHandler::callbacks->begin();
    while (iterator != CallbackHandler::callbacks->end()) {
        repr +=
            R"({"callbackid":")" + iterator->first + R"(", "tableIndexes":[)";
        auto callback = std::begin(*iterator->second);
        while (callback != std::end(*iterator->second)) {
            repr += std::to_string(callback->table_index);
            repr += (++callback != iterator->second->end()) ? ", " : "";
        }
        repr += "]}";
        repr += (++iterator != CallbackHandler::callbacks->end()) ? ", " : "";
    }
    repr += includeOuterCurlyBraces ? "]}" : "]";
    return repr;
}
