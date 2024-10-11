//
// Created by jay on 10/10/24.
//

#include "wasm_simd128.h"

// static unsigned char *memory; // 1 WASM page

__attribute__((import_module("env"), import_name("print_int")))
extern void print_int(int value);

typedef v128_t v128;

__attribute__((noinline))
int add(const int a, const int b) { return a * a + b; }

__attribute__((export_name("main")))
void _start() { print_int(add(4, 2)); }