#pragma once

#include <cstdio>

#include "../WARDuino.h"

#ifndef DEBUG
#define DEBUG 0
#endif
#ifndef INFO
#define INFO 0
#endif
#ifndef WARN
#define WARN 0
#endif
#ifndef TRACE
#define TRACE 0
#endif

/**
 * Endless loop
 */
void end();

#ifdef ARDUINO

#define ASSERT(exp, ...)                                           \
    {                                                              \
        if (!(exp)) {                                              \
            printf("Assert Failed (%s:%d): ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                                   \
            printf("\n\n\nlooping...\n");                          \
            end();                                                 \
        }                                                          \
    }

#define FATAL(...)                                    \
    {                                                 \
        printf("Error(%s:%d): ", __FILE__, __LINE__); \
        printf(__VA_ARGS__);                          \
        printf("\n");                                 \
        end();                                        \
    }

#else

#define ASSERT(exp, ...)                                           \
    {                                                              \
        if (!(exp)) {                                              \
            printf("Assert Failed (%s:%d): ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                                   \
            exit(1);                                               \
        }                                                          \
    }

#if DEBUG
#define FATAL(...)                                      \
    {                                                   \
        printf("Error(%s:%d): \n", __FILE__, __LINE__); \
        printf(__VA_ARGS__);                            \
        exit(1);                                        \
    }
#else
#define FATAL(...)           \
    {                        \
        printf(__VA_ARGS__); \
        exit(1);             \
    }
#endif

#endif

#if DEBUG
#define debug(...)                                       \
    {                                                    \
        printf("DEBUG (%s:%d):\n ", __FILE__, __LINE__); \
        printf(__VA_ARGS__);                             \
    }
#else
#define debug(...) ;
#endif

#if INFO
#define dbg_info(...)        \
    {                        \
        printf(__VA_ARGS__); \
    }
#else
#define dbg_info(...) ;
#endif

#if TRACE
#define dbg_trace(...)       \
    {                        \
        printf(__VA_ARGS__); \
        fflush(stdout);      \
    }
#else
#define dbg_trace(...) ;
#endif

#if WARN
#define dbg_warn(...)        \
    {                        \
        printf(__VA_ARGS__); \
    }
#else
#define dbg_warn(...) ;
#endif

#if DEBUG || TRACE || WARN || INFO
#define _INCLUDE_REPR_CODE 1

char *value_repr(StackValue *v);

char *block_repr(Block *b);

const char *opcode_repr(uint8_t opcode);

#endif

#if DEBUG && TRACE

void dbg_dump_stack(Module *m);

#else
#define dbg_dump_stack(...) ;
#endif
