#pragma once

#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

struct Module;

class Callback;

class Event {
   public:
    std::string topic;
    const char *payload;

    Event(std::string topic, const char *payload);
};

class CallbackHandler {
   private:
    static std::unordered_map<std::string, std::vector<Callback> *> *callbacks;
    static std::deque<Event> *events;

    CallbackHandler() = default;  // Disallow creation

   public:
    static size_t event_count();
    static std::deque<Event>::const_iterator event_begin();
    static std::deque<Event>::const_iterator event_end();

    static bool resolving_event;

    static void add_callback(const Callback &c);
    static void remove_callback(const Callback &c);
    static void push_event(std::string topic, const unsigned char *payload,
                           unsigned int length);
    static void push_event(Event *event);
    static bool resolve_event();

    // WOOD needed to know when to push events
    static bool pushingMode;
};

class Callback {
   private:
    Module *module;  // reference to module
   public:
    std::string topic;
    uint32_t table_index{};

    explicit Callback(Module *m, std::string id, uint32_t tidx);
    Callback(const Callback &c);

    void resolve_event(const Event &e);
};
