#ifndef WASM_TESTS_H
#define WASM_TESTS_H

#include <cstdio>
#include <exception>

extern "C" {
#include "./sexpr-parser/src/sexpr.h"
}

#include "../../src/WARDuino.h"

struct Test {
    SNode *asserts;
    Module *module;
};

int run_wasm_test(WARDuino wac, const char *module_file_path,
                  const char *asserts_file_path, const char *wasm_command);

#endif  // WASM_TESTS_H
