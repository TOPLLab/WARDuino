#ifndef WARDUINO_PRIM_H
#define WARDUINO_PRIM_H

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

bool resolve_external_global(char *symbol, Global **val);

void install_primitives(Interpreter *interpreter);

/*std::vector<IOStateElement *> get_io_state(Module *m);

void restore_external_state(Module *m,
                            const std::vector<IOStateElement> &external_state);*/

inline void create_stack(std::vector<StackValue> *) {}

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
    m->warduino->interpreter->resolve_primitive(function_name.c_str(), &primitive);

    std::vector<StackValue> argStack;
    create_stack(&argStack, args...);

    for (auto arg : argStack) {
        m->stack[++m->sp] = arg;
    }
    primitive(m);
}

#define _install_primitive(prim_name)                                       \
    /*dbg_info("installing primitive number: %d  of %d with name: %s\n", \ \
    prim_index + 1, ALL_PRIMITIVES, #prim_name);              \*/\
    PrimitiveEntry p; \
    p.name = #prim_name;                                          \
    p.t = &(prim_name##_type);                                    \
    p.f = &(prim_name);                                           \
    p.f_reverse = nullptr;                                        \
    p.f_serialize_state = nullptr;                                \
    interpreter->register_primitive(p); \

#define install_primitive(prim_name)                                       \
{                                                                      \
    /*dbg_info("installing primitive number: %d  of %d with name: %s\n", \ \
    prim_index + 1, ALL_PRIMITIVES, #prim_name);              \*/\
    _install_primitive(prim_name) \
}

#define install_reversible_primitive(prim_name)             \
{                                                    \
    _install_primitive(prim_name)    \
    p.f_reverse = &(prim_name##_reverse);           \
    p.f_serialize_state = &(prim_name##_serialize); \
}

#define def_prim(function_name, type) \
    Type function_name##_type = type; \
    bool function_name([[maybe_unused]] Module *m)

#define def_prim_reverse(function_name)     \
    void function_name##_reverse(Module *m, \
                                 std::vector<IOStateElement> external_state)

#define def_prim_serialize(function_name) \
    void function_name##_serialize(       \
        std::vector<IOStateElement *> &external_state)

// TODO: use fp
#define pop_args(n) m->sp -= n
#define get_arg(m, arg) m->stack[(m)->sp - (arg)].value
#define pushUInt32(arg) m->stack[++m->sp].value.uint32 = arg
#define pushInt32(arg) m->stack[++m->sp].value.int32 = arg
#define pushUInt64(arg)                 \
    m->stack[++m->sp].value_type = I64; \
    m->stack[m->sp].value.uint64 = arg
#define arg0 get_arg(m, 0)
#define arg1 get_arg(m, 1)
#define arg2 get_arg(m, 2)
#define arg3 get_arg(m, 3)
#define arg4 get_arg(m, 4)
#define arg5 get_arg(m, 5)
#define arg6 get_arg(m, 6)
#define arg7 get_arg(m, 7)
#define arg8 get_arg(m, 8)
#define arg9 get_arg(m, 9)

#endif
