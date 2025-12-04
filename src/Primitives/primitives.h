#ifndef WARDUINO_PRIM_H
#define WARDUINO_PRIM_H

#include "../Utils/macros.h"
#include "../WARDuino/internals.h"

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
    Primitive primitive = {};
    if (!m->warduino->interpreter->resolve_primitive(function_name.c_str(),
                                                     &primitive)) {
        printf("Failed to resolve primitive %s\n", function_name.c_str());
        return;
    }

    std::vector<StackValue> argStack;
    create_stack(&argStack, args...);

    for (auto arg : argStack) {
        m->stack[++m->sp] = arg;
    }
    primitive(m);
}

#define _init_primitive(prim_name) \
    PrimitiveEntry p = {};         \
    p.name = #prim_name;           \
    p.t = &(prim_name##_type);     \
    p.f = &(prim_name);            \
    p.f_reverse = nullptr;         \
    p.f_serialize_state = nullptr;

#define install_primitive(prim_name)                                   \
    {                                                                  \
        dbg_info("installing primitive %s\n", #prim_name);             \
        _init_primitive(prim_name) interpreter->register_primitive(p); \
    }

#define install_reversible_primitive(prim_name)                          \
    {                                                                    \
        dbg_info("installing reversible primitive %s\n", #prim_name);    \
        _init_primitive(prim_name) p.f_reverse = &(prim_name##_reverse); \
        p.f_serialize_state = &(prim_name##_serialize);                  \
        interpreter->register_primitive(p);                              \
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

inline uint32_t param_arr_len0[0] = {};
inline uint32_t param_I32_arr_len1[1] = {I32};
inline uint32_t param_I32_arr_len2[2] = {I32, I32};
inline uint32_t param_I32_arr_len3[3] = {I32, I32, I32};
inline uint32_t param_I32_arr_len4[4] = {I32, I32, I32, I32};
inline uint32_t param_I32_arr_len10[10] = {I32, I32, I32, I32, I32,
                                           I32, I32, I32, I32, I32};
inline uint32_t param_I64_arr_len1[1] = {I64};

inline Type oneToNoneU32 = {
    .form = FUNC,
    .param_count = 1,
    .params = param_I32_arr_len1,
    .result_count = 0,
    .results = nullptr,
    .mask = 0x8001 /* 0x800 = no return ; 1 = I32*/
};

inline Type twoToNoneU32 = {
    .form = FUNC,
    .param_count = 2,
    .params = param_I32_arr_len2,
    .result_count = 0,
    .results = nullptr,
    .mask = 0x80011 /* 0x800 = no return ; 1 = I32; 1 = I32*/
};

inline Type threeToNoneU32 = {
    .form = FUNC,
    .param_count = 3,
    .params = param_I32_arr_len3,
    .result_count = 0,
    .results = nullptr,
    .mask = 0x800111 /* 0x800 = no return ; 1=I32; 1=I32; 1=I32*/
};

inline Type fourToNoneU32 = {
    .form = FUNC,
    .param_count = 4,
    .params = param_I32_arr_len4,
    .result_count = 0,
    .results = nullptr,
    .mask =
        0x8001111 /* 0x800 = no return ; 1 = I32; 1 = I32; 1 = I32; 1 = I32*/
};

inline Type oneToOneU32 = {
    .form = FUNC,
    .param_count = 1,
    .params = param_I32_arr_len1,
    .result_count = 1,
    .results = param_I32_arr_len1,
    .mask = 0x80011 /* 0x8 1=I32 0=endRet ; 1=I32; 1=I32*/
};

inline Type oneToOneI32 = {
    .form = FUNC,
    .param_count = 1,
    .params = param_I32_arr_len1,
    .result_count = 1,
    .results = param_I32_arr_len1,
    .mask = 0x80011 /* 0x8 1=I32 0=endRet ; 1=I32; 1=I32*/
};

inline Type twoToOneU32 = {
    .form = FUNC,
    .param_count = 2,
    .params = param_I32_arr_len2,
    .result_count = 1,
    .results = param_I32_arr_len1,
    .mask = 0x81011 /* 0x8 1=I32 0=endRet ; 1=I32; 1=I32*/
};

inline Type threeToOneU32 = {
    .form = FUNC,
    .param_count = 3,
    .params = param_I32_arr_len3,
    .result_count = 1,
    .results = param_I32_arr_len1,
    .mask = 0x810111 /* 0x8 1=I32 0=endRet ; 1=I32; 1=I32; 1=I32*/
};

inline Type fourToOneU32 = {
    .form = FUNC,
    .param_count = 4,
    .params = param_I32_arr_len4,
    .result_count = 1,
    .results = param_I32_arr_len1,
    .mask = 0x8101111 /* 0x8 1=I32 0=endRet ; 1=I32; 1=I32; 1=I32; 1=I32*/
};

inline Type tenToOneU32 = {
    .form = FUNC,
    .param_count = 10,
    .params = param_I32_arr_len10,
    .result_count = 1,
    .results = param_I32_arr_len1,
    .mask = 0x8101111111111 /* 0x8 1=I32 0=endRet ; 10 params 1=I32*/
};

inline Type NoneToNoneU32 = {.form = FUNC,
                             .param_count = 0,
                             .params = nullptr,
                             .result_count = 0,
                             .results = nullptr,
                             .mask = 0x80000};

inline Type oneToNoneI32 = {
    .form = FUNC,
    .param_count = 1,
    .params = param_I32_arr_len1,
    .result_count = 0,
    .results = nullptr,
    .mask = 0x8001 /* 0x800 = no return ; 1 = I32*/
};

inline Type NoneToOneU32 = {.form = FUNC,
                            .param_count = 0,
                            .params = nullptr,
                            .result_count = 1,
                            .results = param_I32_arr_len1,
                            .mask = 0x81000};

inline Type NoneToOneU64 = {.form = FUNC,
                            .param_count = 0,
                            .params = nullptr,
                            .result_count = 1,
                            .results = param_I64_arr_len1,
                            .mask = 0x82000};

#endif
