#ifndef WARDUINO_PRIM_H
#define WARDUINO_PRIM_H

#include "../Utils/macros.h"
#include "../Utils/util.h"
#include "../WARDuino.h"

/**
 * Handle import of memory by keeping a array of 256 pages.
 * If more imports of memory are needed, this should be replaced by a
 * nicer function that allows you to specify the different kinds of
 * memory that mey be needed
 *
 * @param symbol name
 * @param val    location to store memory to
 * @return true if memory is found and assigned to val
 */
bool resolve_external_memory(char *symbol, Memory **val);

void install_primitives(Interpreter *interpreter);

std::vector<IOStateElement *> get_io_state(Module *m);

void restore_external_state(Module *m,
                            std::vector<IOStateElement> external_state);

inline void create_stack(std::vector<StackValue> *stack) {}

template <typename T, typename... Ts>
void create_stack(std::vector<StackValue> *stack, T value, Ts... args) {
    StackValue stackValue;
    if constexpr (std::is_same<T, int32_t>() || std::is_same<T, int>()) {
        stackValue.value.int32 = value;
        stackValue.value_type = I32;
    } else if constexpr (std::is_same<T, uint32_t>()) {
        stackValue.value.uint32 = value;
        stackValue.value_type = I32;
    } else if constexpr (std::is_same<T, int64_t>()) {
        stackValue.value.int64 = value;
        stackValue.value_type = I64;
    } else if constexpr (std::is_same<T, uint64_t>()) {
        stackValue.value.uint64 = value;
        stackValue.value_type = I64;
    } else if constexpr (std::is_same<T, float>()) {
        stackValue.value.f32 = value;
        stackValue.value_type = F32;
    } else if constexpr (std::is_same<T, double>()) {
        stackValue.value.f64 = value;
        stackValue.value_type = F64;
    } else {
        // This will trigger a compile time error if a different unsupported
        // type is used.
        static_assert(
            sizeof(T) == 0,
            "Unsupported argument type! Expected i32, i64, f32 or f64.");
    }
    stack->push_back(stackValue);

    create_stack(stack, args...);
}

template <typename... Ts>
void invoke_primitive(Module *m, const std::string &function_name, Ts... args) {
    Primitive primitive;
    m->warduino->interpreter->find_primitive(function_name.c_str(), &primitive);

    std::vector<StackValue> argStack;
    create_stack(&argStack, args...);

    for (auto arg : argStack) {
        m->stack[++m->sp] = arg;
    }
    primitive(m);
}

//------------------------------------------------------
// Restore external state when restoring a snapshot
//------------------------------------------------------
inline void restore_external_state(Module *m,
                            std::vector<IOStateElement> external_state) {
    uint8_t opcode = *m->pc_ptr;
    // TODO: Maybe primitives can also be called using the other call
    // instructions such as call_indirect
    //  maybe there should just be a function that checks if a certain function
    //  is being called that handles all these cases?
    if (opcode == 0x10) {  // call opcode
        uint8_t *pc_copy = m->pc_ptr + 1;
        uint32_t fidx = read_LEB_32(&pc_copy);
        if (fidx < m->import_count) {
            for (auto &primitive : m->warduino->interpreter->primitives) {
                if (!strcmp(primitive.name, m->functions[fidx].import_field)) {
                    if (primitive.f_reverse) {
                        debug("Reversing action for primitive %s\n",
                              primitive.name);
                        primitive.f_reverse(m, external_state);
                    }
                    return;
                }
            }
        }
    }
}

//------------------------------------------------------
// Serialize external state into a snapshot
//------------------------------------------------------
inline std::vector<IOStateElement *> get_io_state(Module *m) {
    std::vector<IOStateElement *> ioState;
    for (auto &primitive : m->warduino->interpreter->primitives) {
        if (primitive.f_serialize_state) {
            primitive.f_serialize_state(ioState);
        }
    }
    return ioState;
}

#endif
