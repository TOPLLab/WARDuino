#include <string.h>

#include <iostream>

#include "../debug.h"
#include "../src/WARDuino.h"
#include "timer.h"

#define MAIN "bench"
#define MAX_PATH 100
#define MAX_BYTE_CODE_SIZE 10000
#define BENCHMARK_PATH "./tasks/"

typedef const char *string;

void set_path(char *path, string name) {
    strncpy(path, BENCHMARK_PATH, strlen(BENCHMARK_PATH) + 1);
    strncat(path, name, strlen(name) + 1);
    strncat(path, "/wast/impl.wasm", 15 + 1);
}

unsigned int read_file_to_buf(unsigned char *bytes, string path) {
    FILE *file = fopen(path, "r");
    if (file == nullptr) {
        fprintf(stderr, "Cannot open file: %s", path);
        exit(1);
    }
    fseek(file, 0L, SEEK_END);
    long num_bytes = ftell(file);
    ASSERT(num_bytes > 0, "Could not Ftell");
    if (num_bytes < MAX_BYTE_CODE_SIZE) {
        fseek(file, 0L, SEEK_SET);
        size_t result = fread(bytes, sizeof(char), (size_t)num_bytes, file);
        if (result != (size_t)num_bytes) {
            fprintf(stderr, "reading error while loading file %s", path);
            exit(1);
        }
        fclose(file);
        return (unsigned int)num_bytes;
    } else {
        fprintf(stderr, "File  < %s  > is too big for buffer", path);
        exit(1);
    }
}

int run_benchmarks(size_t num_benchmarks, string benchmarks[],
                   uint32_t expected[]) {
    char path[MAX_PATH];
    unsigned char bytes[MAX_BYTE_CODE_SIZE];
    unsigned int bytes_length;
    auto *w = new WARDuino();
    size_t correct = 0;
    for (size_t i = 0; i < num_benchmarks; i++) {
        string name = benchmarks[i];
        set_path(path, name);
        printf("[%lu/%lu: GO ] %s \n", i, num_benchmarks, path);
        bytes_length = read_file_to_buf(bytes, path);
        Options opt = {.disable_memory_bounds = false,
                       .mangle_table_index = false,
                       .dlsym_trim_underscore = true,
                       .return_exception = false};
        Timer tmr;
        tmr.reset();
        Module *m = w->load_module(bytes, bytes_length, opt);
        uint32_t fidx = w->get_export_fidx(m, MAIN);
        if (fidx != static_cast<uint32_t>(-1)) {
            printf("[%lu/%lu:LOAD] %s , fidx=%u\n", i, num_benchmarks, path,
                   fidx);

            double load = tmr.elapsed();
            bool succeed = true;
            for (int j = 0; j < 10 && succeed; ++j) {
                succeed = succeed && w->invoke(m, fidx);
            }
            double total = tmr.elapsed();
            if (!succeed) {
                printf("[%lu/%lu:FAIL] %s could not be interpreted\n", i,
                       num_benchmarks, path);
                exit(1);
            } else {
                uint32_t res = m->stack->value.uint32;
                if (res == expected[i]) {
                    printf(
                        "[%lu/%lu: OK ] %s (output: 0x%x = %u, load module: "
                        "%fs, total: "
                        "%fs)\n",
                        i + 1, num_benchmarks, path, res, res, load, total);
                    correct++;
                } else {
                    printf(
                        "[%lu/%lu:FAIL] %s (output: 0x%x = %u (!= %u), load "
                        "module: "
                        "%fs, total: "
                        "%fs)\n",
                        i + 1, num_benchmarks, path, res, res, expected[i],
                        load, total);
                }
            }
        } else {
            printf("[%lu/%lu:FAIL] %s did not have an exported function " MAIN
                   "\n",
                   i, num_benchmarks, path);
        }
        w->unload_module(m);
    }
    return correct;
}

int main(int argc, const char *argv[]) {
    string benchmarks[] = {"tak", "fib", "fac", "gcd", "catalan", "primes"};
    uint32_t expected[] = {7, 91, 82, 62882, 244, 1824};
    size_t num = (size_t)(sizeof(benchmarks) / sizeof(string *));
    size_t correct = run_benchmarks(num, benchmarks, expected);
    bool pass = (num == correct);
    printf("SUMMARY: %s (%zu / %zu)\n", pass ? "PASS" : "FAIL", correct, num);
    return pass ? 0 : 1;
}
