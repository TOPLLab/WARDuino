#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <cmath>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>

extern "C" {
#include "./sexpr-parser/src/sexpr.h"
}

#include "../debug.h"
#include "../instructions.h"
#include "assertion.h"

#define COMPILE(command) system((command).c_str());

WARDuino wac;

volatile bool handlingInterrupt = false;

void signalHandler(int /* signum */) {
    if (handlingInterrupt) return;

    printf("CHANGE REQUESTED!");
    struct stat statbuff {};
    if (stat("/tmp/change", &statbuff) == 0 && statbuff.st_size > 0) {
        auto *data = (uint8_t *)malloc(statbuff.st_size * sizeof(uint8_t));
        FILE *fp = fopen("/tmp/change", "rb");
        fread(data, statbuff.st_size, 1, fp);
        fclose(fp);
        wac.handleInterrupt(statbuff.st_size, data);
    }

    handlingInterrupt = false;
}

uint8_t *mmap_file(char *path, int *len) {
    int fd;
    int res;
    struct stat sb;
    uint8_t *bytes;

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        FATAL("could not open file '%s'\n", path);
    }
    res = fstat(fd, &sb);
    if (res < 0) {
        FATAL("could not stat file '%s' (%d)\n", path, res);
    }

    bytes = (uint8_t *)mmap(nullptr, sb.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (len) {
        *len = sb.st_size;  // Return length if requested
    }
    if (bytes == MAP_FAILED) {
        FATAL("could not mmap file '%s'", path);
    }
    return bytes;
}

// Parse and add arguments to the stack
void parse_args(Module *m, Type *type, int argc, Value argv[]) {
    for (int i = 0; i < argc; i++) {
        m->sp++;
        StackValue *sv = &m->stack[m->sp];
        sv->value_type = type->params[i];
        switch (type->params[i]) {
            case I64:
                sv->value.int64 = argv[i].int64;
                break;
            case I32:
                sv->value.int32 = argv[i].int32;
                break;
            case F32:
                sv->value.f32 = argv[i].f32;
                break;
            case F64:
                sv->value.f64 = argv[i].f64;
                break;
        }
    }
}

void invoke(Module *m, const char *call_f, Value values[]) {
    uint32_t fidx = -1;
    m->sp = -1;
    m->fp = -1;
    m->csp = -1;
    // TODO move to the WARDuino class
    for (uint32_t f = 0; f < m->function_count; f++) {
        char *fname = m->functions[f].export_name;
        if (!fname) {
            continue;
        }
        if (strncmp(call_f, fname, 1024) == 0) {
            fidx = f;
            break;
        }
    }

    Block *func = &m->functions[fidx];
    Type *type = func->type;
    parse_args(m, type, type->param_count, values);
    setup_call(m, fidx);
    m->options.return_exception = true;
    interpret(m);
}

void assertValue(Value *val, Module *m) {
    switch (val->type) {
        case I64V:
            printf("result :: %lu ", m->stack->value.uint64);
            if (val->uint64 == m->stack->value.uint64) {
                printf("OK\n");
            } else {
                printf("FAIL\n");
            }
            break;
        case I32V:
            printf("result :: %d ", m->stack->value.int32);
            if (val->int32 == m->stack->value.int32) {
                printf("OK\n");
            } else {
                printf("FAIL\n");
            }
            break;
        case F32V:
            printf("result :: %f ", m->stack->value.f32);
            if (val->f32 == m->stack->value.f32 ||
                (std::isnan(val->f32) && std::isnan(m->stack->value.f32))) {
                printf("OK\n");
            } else {
                printf("FAIL\n");
            }
            break;
        case F64V:
            printf("result :: %f ", m->stack->value.f64);
            if (val->f64 == m->stack->value.f64 ||
                (std::isnan(val->f64) && std::isnan(m->stack->value.f64))) {
                printf("OK\n");
            } else {
                printf("FAIL\n");
            }
            break;
        default:
            printf("Error unsupported value");
            exit(1);
    }
}

void assertResult(Result *result, Module *m) {
    switch (result->type) {
        case VAL:
            assertValue(result->value, m);
            break;
        default:
            printf("Error unsupported result");
            exit(1);
    }
}

void assertException(char *expected, Module *m) {
    printf("result :: %s ", m->exception);
    if (strcmp(m->exception, expected) == 0) {
        printf("OK\n");
    } else {
        printf("FAIL\n");
    }
}

void runAction(Action *action, Module *m) {
    switch (action->type) {
        case INVOKE:
            invoke(m, action->name, action->expr);
            break;
        default:
            printf("Error unsupported action");
            exit(1);
    }
}

void runAssertion(Assertion *assertion, Module *m) {
    switch (assertion->type) {
        case RETURN:
            runAction(assertion->action, m);
            assertResult(assertion->result, m);
            break;
        case EXHAUSTION:
            runAction(assertion->action, m);
            assertException("call stack exhausted", m);
            break;
        default:
            printf("Error unsupported assertion");
            exit(1);
    }
}

Result *parseResultNode(SNode *node) {
    Value *value = nullptr;

    if (node->type == STRING) {
        value = makeSTR(node->value);
    } else if (strcmp(node->list->value, "i64.const") == 0) {
        value = makeI64(std::stoll(node->list->next->value, nullptr, 0));
    } else if (strcmp(node->list->value, "u64.const") == 0) {
        value = makeUI64(std::stoull(node->list->next->value, nullptr, 0));
    } else if (strcmp(node->list->value, "i32.const") == 0) {
        value = makeI32(std::stol(node->list->next->value, nullptr, 0));
    } else if (strcmp(node->list->value, "f32.const") == 0) {
        value = makeF32(std::stof(node->list->next->value, nullptr));
    } else if (strcmp(node->list->value, "f64.const") == 0) {
        value = makeF64(std::stod(node->list->next->value, nullptr));
    } else {
        // TODO
    }

    return makeValueResult(value);
}

Action *parseActionNode(SNode *actionNode) {
    char *name = actionNode->list->next->value;
    SNode *param = actionNode->list->next->next;

    std::vector<Value> params;

    while (param != nullptr) {
        char *type = param->list->value;
        char *value = param->list->next->value;

        if (strcmp(type, "i64.const") == 0) {
            params.push_back(*makeI64(std::stoll(value, nullptr, 0)));
        } else if (strcmp(type, "u64.const") == 0) {
            params.push_back(*makeUI64(std::stoull(value, nullptr, 0)));
        } else if (strcmp(type, "i32.const") == 0) {
            params.push_back(*makeI32(std::stol(value, nullptr, 0)));
        } else if (strcmp(type, "f32.const") == 0) {
            params.push_back(*makeF32(std::stof(value, nullptr)));
        } else if (strcmp(type, "f64.const") == 0) {
            params.push_back(*makeF64(std::stod(value, nullptr)));
        } else {
            // TODO
        }
        param = param->next;
    }

    auto *args = (Value *)calloc(sizeof(Value), params.size());
    copy(params.begin(), params.end(), args);

    return makeInvokeAction(name, args);
}

void resolveAssert(SNode *node, Module *m) {
    char *assertType = node->value;

    if (strcmp(assertType, "assert_return") == 0) {
        Action *action = parseActionNode(node->next);
        Result *result = parseResultNode(node->next->next);

        Assertion *assertion = makeAssertionReturn(action, result);
        printf("assert return value:\n");

        runAssertion(assertion, m);

        free(result);
        free(action);
        free(assertion);
    } else if (strcmp(assertType, "assert_exhaustion") == 0) {
        Action *action = parseActionNode(node->next);
        Assertion *assertion = makeAssertionExhaustion(action);
        printf("assert stack exhaustion:\n");

        runAssertion(assertion, m);

        free(action);
        free(assertion);
    } else {
        // TODO
    }
}

/**
 * Run code, execute interrupts in /tmp/change if a USR1 signal comes
 */
int main(int argc, char **argv) {
    uint8_t *bytes;
    int byte_count;
    signal(SIGUSR1, signalHandler);

    // Load the path name
    char *wast_path = argv[1];
    char *tests_path = argv[2];
    char *wasm_command = argv[3];

    // Compile wasm program
    std::string out_path = wast_path;
    out_path += ".wasm";
    std::string command = wasm_command;
    command += " ";
    command += wast_path;
    command += " -o ";
    command += out_path;
    COMPILE(command);

    // Load wasm program
    bytes = mmap_file(&out_path[0], &byte_count);

    if (bytes == nullptr) {
        fprintf(stderr, "Could not load %s", out_path.c_str());
        return 2;
    }

    Module *m = wac.load_module(bytes, byte_count, {});

    // Parse asserts as s-expressions
    FILE *fp = fopen(tests_path, "r");
    if (fp == nullptr) {
        fprintf(stderr, "Could not open %s", tests_path);
        return 2;
    }
    struct SNode *node = snode_parse(fp);
    fclose(fp);

    // Loop over all asserts in the file
    struct SNode *cursor = node;
    while (cursor != nullptr) {
        switch (cursor->type) {
            case LIST:
                resolveAssert(cursor->list, m);
                cursor = cursor->next;
                break;
            case SYMBOL:
                // ignore comments
                cursor = cursor->next;
                break;
            case STRING:
            case INTEGER:
            case FLOAT:
                printf("Error badly formed asserts");
                exit(1);
            default:
                printf("Error unsupported type");
                exit(1);
        }
    }

    // Remove compiled file
    remove(&out_path[0]);

    return 0;
}
