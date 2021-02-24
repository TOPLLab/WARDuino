#include <iostream>
#include "../debug.h"
#include "../WARDuino.h"
#include "../WARDuino.h"
#include "../instructions.h"
#include <csignal>
#include <sys/mman.h>
#include <cstdlib>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "./sexpr-parser/src/sexpr.h"
}

#include "assertion.h"

extern "C" {
// TODO: Stat files, alternative needed for arduino
#include <sys/stat.h>
// END
}

WARDuino wac;

volatile bool handlingInterrupt = false;

void signalHandler(int /* signum */) {
    if (handlingInterrupt) return;

    printf("CHANGE REQUESTED!");
    struct stat statbuff{};
    if (stat("/tmp/change", &statbuff) == 0 && statbuff.st_size > 0) {
        auto *data = (uint8_t *) malloc(statbuff.st_size * sizeof(uint8_t));
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
    if (fd < 0) {FATAL("could not open file '%s'\n", path); }
    res = fstat(fd, &sb);
    if (res < 0) {FATAL("could not stat file '%s' (%d)\n", path, res); }

    bytes = (uint8_t *) mmap(0, sb.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (len) {
        *len = sb.st_size;  // Return length if requested
    }
    if (bytes == MAP_FAILED) {FATAL("could not mmap file '%s'", path); }
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
                sv->value.uint64 = argv[i].int64;
                break;
/*
        case I32: sv->value.uint32 = strtoul(argv[i], NULL, 0); break;
        case F32: if (strncmp("-nan", argv[i], 4) == 0) {
                      sv->value.f32 = -NAN;
                  } else {
                      sv->value.f32 = atof(argv[i]);
                  }; break;
        case F64: if (strncmp("-nan", argv[i], 4) == 0) {
                      sv->value.f64 = -NAN;
                  } else {
                      sv->value.f64 = atof(argv[i]);
                  }; break;*/
        }
    }
}

void invoke(Module *m, const char *call_f, Value values[]) {
    uint32_t fidx = -1;
    m->sp = -1;
    m->fp = -1;
    m->csp = -1;
    //TODO move to the WARDuino class     
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
    interpret(m);

    printf("result :: %llu ", m->stack->value.uint64);
}

void assertValue(Value *val, Module *m) {
    switch (val->type) {
        case I64V:
            if (val->uint64 == m->stack->value.uint64) {
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

void runAction(Action *action, Module *m, Result *result) {
    switch (action->type) {
        case INVOKE:
            invoke(m, action->name, action->expr);
            assertResult(result, m);
            break;
        default:
            printf("Error unsupported action");
            exit(1);
    }

}

void runAssertion(Assertion *assertion, Module *m) {
    switch (assertion->type) {
        case RETURN:
            runAction(assertion->action, m, assertion->result);
            break;
        default:
            printf("Error unsupported assertion");
            exit(1);
    }
}

Result *parseResultNode(SNode *node) {
    Value *value = nullptr;

    if (strcmp(node->list->value, "i64.const") == 0) {
        value = makeI64(std::stoll(node->list->next->value));
    } else if (strcmp(node->list->value, "u64.const") == 0) {
        value = makeUI64(std::stoull(node->list->next->value));
    } else {
        // TODO
    }

    return makeValueResult(value);
}

Action *parseActionNode(SNode *actionNode) {
    char *name = actionNode->list->next->value;
    char *type = actionNode->list->next->next->list->value;
    char *value = actionNode->list->next->next->list->next->value;

    auto *args = (Value *) calloc(sizeof(Value), 1);
    if (strcmp(type, "i64.const") == 0) {
        args[0] = *makeI64(std::stoll(value));
    } else if (strcmp(type, "u64.const") == 0) {
        args[0] = *makeUI64(std::stoull(value));
    } else {
        // TODO
    }

    return makeInvokeAction(name, args);
}

void resolveAssert(SNode *node, Module *m) {
    // resolve (assert_return (invoke "fac-rec" (i64.const 25)) (i64.const 7034535277573963776))
    char *assertType = node->value;
    if (strcmp(assertType, "assert_return") == 0) {
        SNode *actionNode = node->next;
        Action *action = parseActionNode(actionNode);  // TODO breaks invoke?
        SNode *resultNode = actionNode->next;
        Result *result = parseResultNode(resultNode);

        Assertion *assertion = makeAssertionReturn(action, result);
        printf("made an assertion\n");

        runAssertion(assertion, m);

        free(result);
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
    char *mod_path = argv[1];
    char *tests_path = argv[2];

    // Load wasm program
    bytes = mmap_file(mod_path, &byte_count);

    if (bytes == nullptr) {
        fprintf(stderr, "Could not load %s", mod_path);
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
            case STRING:
            case SYMBOL:
            case INTEGER:
            case FLOAT:
                printf("Error badly formed asserts");
                exit(1);
            default:
                printf("Error unsupported type");
                exit(1);
        }
        if (cursor == nullptr || cursor->next == nullptr) {
            break;
        }
    }

    return 0;
}
