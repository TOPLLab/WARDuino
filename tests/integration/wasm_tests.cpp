#include "wasm_tests.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "../../debug.h"
#include "../../instructions.h"
#include "assertion.h"

int COUNT = 0;

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

bool assertValue(Value *val, Module *m) {
    switch (val->type) {
        case I64V:
            printf("result :: %lu ", m->stack->value.uint64);
            if (val->uint64 == m->stack->value.uint64) {
                printf("OK\n");
                return true;
            } else {
                printf("FAIL\n");
                return false;
            }
        case I32V:
            printf("result :: %d ", m->stack->value.int32);
            if (val->int32 == m->stack->value.int32) {
                printf("OK\n");
                return true;
            } else {
                printf("FAIL\n");
                return false;
            }
            break;
        case F32V:
            printf("result :: %f ", m->stack->value.f32);
            if (val->f32 == m->stack->value.f32 ||
                (std::isnan(val->f32) && std::isnan(m->stack->value.f32))) {
                printf("OK\n");
                return true;
            } else {
                printf("FAIL\n");
                return false;
            }
            break;
        case F64V:
            printf("result :: %f ", m->stack->value.f64);
            if (val->f64 == m->stack->value.f64 ||
                (std::isnan(val->f64) && std::isnan(m->stack->value.f64))) {
                printf("OK\n");
                return true;
            } else {
                printf("FAIL\n");
                return false;
            }
            break;
        default:
            printf("Error unsupported value");
            exit(1);
    }
}

bool assertResult(Result *result, Module *m) {
    if (m->exception != nullptr) {
        printf("exception :: %s FAIL\n", m->exception);
        return false;
    }
    switch (result->type) {
        case VAL:
            return assertValue(result->value, m);
        default:
            printf("Error unsupported result");
            exit(1);
    }
}

bool assertException(char *expected, Module *m) {
    printf("exception :: %s ", m->exception);
    if (strcmp(m->exception, expected) == 0) {
        printf("OK\n");
        return true;
    } else {
        printf("FAIL\n");
        return false;
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

bool runAssertion(Assertion *assertion, Module *m) {
    switch (assertion->type) {
        case RETURN:
            runAction(assertion->action, m);
            return assertResult(assertion->result, m);
        case EXHAUSTION:
            runAction(assertion->action, m);
            return assertException("call stack exhausted", m);
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

bool resolveAssert(SNode *node, Module *m) {
    char *assertType = node->value;
    bool success = false;
    if (strcmp(assertType, "assert_return") == 0) {
        Action *action = parseActionNode(node->next);
        Result *result = parseResultNode(node->next->next);

        Assertion *assertion = makeAssertionReturn(action, result);
        printf("%i. assert return %s:\n", COUNT++, action->name);

        success = runAssertion(assertion, m);

        free(result);
        free(action);
        free(assertion);
    } else if (strcmp(assertType, "assert_exhaustion") == 0) {
        Action *action = parseActionNode(node->next);
        Assertion *assertion = makeAssertionExhaustion(action);
        printf("assert stack exhaustion:\n");

        success = runAssertion(assertion, m);

        free(action);
        free(assertion);
    } else {
        // TODO
    }
    return success;
}

int init_module(WARDuino wac, Test *test, const std::string &module_file_path,
                std::string &output_path, const std::string &wasm_command) {
    // Compile wasm program
    std::string command =
        wasm_command + " " + module_file_path + " -o " + output_path;
    int return_code = system((command).c_str());
    if (return_code != 0) {
        fprintf(stderr, "Error: \"%s\" failed to compile test.\n",
                wasm_command.c_str());
        return return_code;
    }

    // Load wasm program
    int byte_count;
    uint8_t *bytes = mmap_file(&output_path[0], &byte_count);

    if (bytes == nullptr) {
        fprintf(stderr, "Could not load %s", output_path.c_str());
        return 1;
    }

    test->module = wac.load_module(bytes, byte_count, {});

    return 0;
}

int run_wasm_test(WARDuino wac, char *module_file_path, char *asserts_file_path,
                  char *wasm_command) {
    FILE *asserts_file = fopen(asserts_file_path, "r");
    auto *test = (Test *)calloc(1, sizeof(Test));
    if (asserts_file == nullptr || test == nullptr) {
        return 1;
    }

    std::string output_path = module_file_path;
    output_path += ".wasm";
    int ret = init_module(std::move(wac), test, module_file_path, output_path,
                          wasm_command);
    if (ret != 0) {
        return ret;
    }

    test->asserts = snode_parse(asserts_file);

    fclose(asserts_file);

    // Loop over all asserts in the file
    bool all_tests_passed = true;
    struct SNode *cursor = test->asserts;
    while (cursor != nullptr) {
        switch (cursor->type) {
            case LIST:
                all_tests_passed &= resolveAssert(cursor->list, test->module);
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
    remove(&output_path[0]);

    if (all_tests_passed) {
        printf("All tests passed.\n");
    } else {
        printf("Some tests failed.\n");
    }

    return all_tests_passed ? 0 : 2;
}
