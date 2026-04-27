#include "CallbackHandler.h"

#include <algorithm>

#include "../Debug/debugger.h"
#include "../Interpreter/interpreter.h"
#include "../Utils/macros.h"
#include "../WARDuino.h"

void push_guard(Module *m) {
    if (m == nullptr) {
        return;
    }
    auto *guard = static_cast<Block *>(malloc(sizeof(Block)));
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
    WARDuino::instance()->interpreter->push_block(m, guard, m->sp);
}

// CallbackHandler class

bool CallbackHandler::manual_event_resolution = false;
bool CallbackHandler::resolving_event = false;
size_t CallbackHandler::pushed_cursor = 0;

bool should_push_event() {
    return WARDuino::instance()->program_state == PROXYrun ||
           WARDuino::instance()->program_state == PROXYhalt;
}

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

// key counter starts at 1
// reset every time the interrupt_mask_topic_to_keys is emptied
uint32_t CallbackHandler::interrupt_mask_fresh_key = 1;

// map of < event group , ( set of disable keys , set of discard keys ) >
std::unordered_map<EventGroup, std::pair<std::unordered_set<uint32_t>,
                                         std::unordered_set<uint32_t>>>
    *CallbackHandler::event_group_to_keys =
        new std::unordered_map<EventGroup,
                               std::pair<std::unordered_set<uint32_t>,
                                         std::unordered_set<uint32_t>>>();

// ( set of disable keys , set of discard keys ) that impact ALL events
std::pair<std::unordered_set<uint32_t>, std::unordered_set<uint32_t>>
    *CallbackHandler::all_event_groups_keys =
        new std::pair<std::unordered_set<uint32_t>,
                      std::unordered_set<uint32_t>>();

uint32_t CallbackHandler::mask_interrupt(EventGroup group, bool discard) {
    uint32_t key = CallbackHandler::interrupt_mask_fresh_key++;
    bool all_events = static_cast<uint8_t>(group) == 0xff;
    auto &pair_of_keys =
        all_events ? *all_event_groups_keys : (*event_group_to_keys)[group];
    std::unordered_set<uint32_t> &event_keys =
        discard ? pair_of_keys.second : pair_of_keys.first;
    event_keys.insert(key);
    printf("Masked interrupt with group %u using key %u (discard: %u)\n", group,
           key, discard);
    printf(
        "All event groups keys now has %zu disable keys and %zu discard keys\n",
        all_event_groups_keys->first.size(),
        all_event_groups_keys->second.size());
    return key;
}

void CallbackHandler::unmask_interrupt(uint32_t key) {
    std::vector<EventGroup> groups_to_remove;

    for (auto *keys :
         {&all_event_groups_keys->first, &all_event_groups_keys->second}) {
        if (keys->erase(key)) {
            return;
        }
    }

    for (auto &[group, pair] : *event_group_to_keys) {
        for (auto *keys : {&pair.first, &pair.second}) {
            if (keys->erase(key)) {
                if (pair.first.empty() && pair.second.empty()) {
                    groups_to_remove.push_back(group);
                }
                break;
            }
        }
    }

    for (auto g : groups_to_remove) {
        event_group_to_keys->erase(g);
    }
}

// WARNING: Push event functions should not use IO functions, since they can be
// called from ISR callbacks
void CallbackHandler::push_event(std::string topic, EventGroup group,
                                 const char *payload,
                                 const unsigned int length) {
    CallbackHandler::push_event(
        new Event(std::move(topic), group, std::string(payload, length)));
}

void CallbackHandler::push_event(Event *event) {
    // check if discarded for all events
    auto &disable_all_groups_keys = all_event_groups_keys->second;
    if (disable_all_groups_keys.size()) {
        printf("Not pushing event with topic %s and group %u\n",
               event->topic.c_str(), event->group);
        return;
    }

    // check if discarded for event group
    auto &all_discard_keys = all_event_groups_keys->second;
    auto entry = CallbackHandler::event_group_to_keys->find(event->group);
    auto keys = entry != CallbackHandler::event_group_to_keys->end()
                    ? &entry->second.second
                    : nullptr;
    if (keys != nullptr &&
        !keys->empty()) {  // second check is actually redundant
        debug("Not pushing event with topic %s and group %u\n",
              event->topic.c_str(), event->group);
        return;
    }

    // push the event if not discarded
    if (events->size() < EVENTS_SIZE) {
        events->push_back(*event);
    }
}

bool CallbackHandler::resolve_event(bool force) {
    if ((!force && CallbackHandler::resolving_event) ||
        CallbackHandler::events->empty()) {
        if (force) {
            printf("No events to be processed!\n");
            WARDuino::instance()->debugger->channel->write(
                "no events to be processed");
        }
        return false;
    }

    // check if masked for all events
    auto &disable_all_groups_keys = all_event_groups_keys->first;
    if (disable_all_groups_keys.size()) {
        debug(
            "Event with topic %s and group %u is masked for all event "
            "groups.\n",
            CallbackHandler::events->front().topic.c_str(),
            CallbackHandler::events->front().group);
        return false;
    }

    // check if masked for event group
    Event event = CallbackHandler::events->front();
    auto entry = event_group_to_keys->find(event.group);
    auto keys =
        entry != event_group_to_keys->end() ? &entry->second.first : nullptr;
    if (keys != nullptr &&
        !keys->empty()) {  // second check is acutally redundant
        debug("Event with topic %s and group %u is masked.\n",
              event.topic.c_str(), event.group);
        return false;
    }

    if (should_push_event()) {
        Event e = CallbackHandler::events->at(CallbackHandler::pushed_cursor++);
        WARDuino::instance()->debugger->channel->write(
            R"({"topic":"%s","payload":"%s"})", e.topic.c_str(),
            e.payload.c_str());

        CallbackHandler::events->pop_front();
        CallbackHandler::pushed_cursor--;
        CallbackHandler::resolving_event = false;
        return !CallbackHandler::events->empty();
        // no further execution if drone
    }

    if (!force && (CallbackHandler::manual_event_resolution ||
                   WARDuino::instance()->program_state == WARDUINOpause)) {
        return true;
    }

    CallbackHandler::resolving_event = true;
    CallbackHandler::events->pop_front();

    debug("Resolving an event. (%zu remaining)\n",
          CallbackHandler::events->size());

    auto iterator = CallbackHandler::callbacks->find(event.topic);
    if (iterator != CallbackHandler::callbacks->end()) {
        Module *module = nullptr;
        std::string key = iterator->first;
        for (Callback cbs : *iterator->second) {
            cbs.resolve_event(event);
            module = cbs.module;
        }
        push_guard(module);
    } else {
        // TODO handle error: event for non-existing iterator
        printf("No handler found for %s (in %zu items)!\n", event.topic.c_str(),
               CallbackHandler::callbacks->size());
    }
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

// Callback class

Callback::Callback(Module *m, std::string id, uint32_t tidx) {
    this->module = m;
    this->topic = std::move(id);
    this->table_index = tidx;
}

void Callback::resolve_event(const Event &e) {
    dbg_trace("Callback(%s, %i): resolving Event(%s, \"%s\")\n", topic.c_str(),
              table_index, e.topic.c_str(), e.payload.c_str());

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
    module->stack[module->sp].value_type = I32;
    module->stack[++module->sp].value.uint32 = topic.length();
    module->stack[module->sp].value_type = I32;
    module->stack[++module->sp].value.uint32 = start;
    module->stack[module->sp].value_type = I32;
    module->stack[++module->sp].value.uint32 = payload.length();
    module->stack[module->sp].value_type = I32;
    module->stack[++module->sp].value.uint32 = payload.length();
    module->stack[module->sp].value_type = I32;

    // Setup function
    uint32_t fidx = module->table.entries[table_index];
    WARDuino::instance()->interpreter->setup_call(module, fidx);

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

Event::Event(std::string topic, EventGroup group, std::string payload) {
    this->topic = topic;
    this->group = group;
    this->payload = payload;
}

std::string Event::serialized() const {
    return R"({"topic": ")" + this->topic + R"(", "group": ")" +
           std::to_string(static_cast<uint8_t>(this->group)) +
           R"(", "payload": ")" + this->payload + R"("})";
}
