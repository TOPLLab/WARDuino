#pragma once

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
#define get_ectx(m) (m->warduino->execution_context)
#define pop_args(n) get_ectx(m)->sp -= n
#define get_arg(m, arg) get_ectx(m)->stack[get_ectx(m)->sp - (arg)].value
#define pushUInt32(arg) get_ectx(m)->stack[++get_ectx(m)->sp].value.uint32 = arg
#define pushInt32(arg) get_ectx(m)->stack[++get_ectx(m)->sp].value.int32 = arg
#define pushUInt64(arg)                                     \
    get_ectx(m)->stack[++get_ectx(m)->sp].value_type = I64; \
    get_ectx(m)->stack[get_ectx(m)->sp].value.uint64 = arg
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
inline uint32_t param_I32_arr_len5[5] = {I32, I32, I32, I32, I32};
inline uint32_t param_I32_arr_len6[6] = {I32, I32, I32, I32, I32, I32};
inline uint32_t param_I32_arr_len7[7] = {I32, I32, I32, I32, I32, I32, I32};
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

inline Type fiveToNoneU32 = {
    .form = FUNC,
    .param_count = 5,
    .params = param_I32_arr_len5,
    .result_count = 0,
    .results = nullptr,
    .mask = 0x80011111 /* 0x800 = no return ; 1 = I32 ; 1 = I32; 1 = I32; 1 =
                          I32; 1 = I32*/
};

inline Type sixToNoneU32 = {
    .form = FUNC,
    .param_count = 6,
    .params = param_I32_arr_len6,
    .result_count = 0,
    .results = nullptr,
    .mask = 0x800111111 /* 0x800 = no return ; 1 = I32 ; 1 = I32 ; 1 = I32; 1 =
                          I32; 1 = I32; 1 = I32*/
};

inline Type sevenToNoneU32 = {
    .form = FUNC,
    .param_count = 7,
    .params = param_I32_arr_len7,
    .result_count = 0,
    .results = nullptr,
    .mask = 0x8001111111 /* 0x800 = no return ; 1 = I32 ; 1 = I32 ; 1 = I32; 1 =
                          I32; 1 = I32; 1 = I32; 1 = I32*/
};

inline Type zeroToOneU32 = {
    .form = FUNC,
    .param_count = 0,
    .params = param_arr_len0,
    .result_count = 1,
    .results = param_I32_arr_len1,
    .mask = 0x810 /* 0x8 1=I32 0=endRet ; no params*/
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

inline Type sixToOneU32 = {
    .form = FUNC,
    .param_count = 6,
    .params = param_I32_arr_len6,
    .result_count = 1,
    .results = param_I32_arr_len1,
    .mask = 0x810111111 /* 0x8 1=I32 0=endRet ; 1=I32; 1=I32; 1=I32; 1=I32; 1=I32; 1=I32*/
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
