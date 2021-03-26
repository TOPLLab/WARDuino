#ifndef WASM_TESTS_H
#define WASM_TESTS_H

#include <cstdio>
#include <exception>

extern "C" {
#include "./sexpr-parser/src/sexpr.h"
}

#include "../../WARDuino.h"

struct Test {
    SNode *asserts;
    Module *module;
};

int run_wasm_test(WARDuino wac, char *module_file_path, char *asserts_file_path,
                  char *wasm_command);

#endif  // WASM_TESTS_H
