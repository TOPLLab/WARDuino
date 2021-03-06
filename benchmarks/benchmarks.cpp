#include <string.h>
#include <iostream>
#include "../WARDuino.h"
#include "timer.h"
#include "../debug.h"

#define MAIN "main"
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
        size_t result = fread(bytes, sizeof(char), (size_t) num_bytes, file);
        if (result != (size_t) num_bytes) {
            fprintf(stderr, "reading error while loading file %s", path);
            exit(1);
        }
        fclose(file);
        return (unsigned int) num_bytes;
    } else {
        fprintf(stderr, "File  < %s  > is too big for buffer", path);
        exit(1);
    }
}

void run_benchmarks(size_t num_benchmarks, string benchmarks[]) {
    char path[MAX_PATH];
    unsigned char bytes[MAX_BYTE_CODE_SIZE];
    unsigned int bytes_length;
    auto *w = new WARDuino();

    for (size_t i = 0; i < num_benchmarks; i++) {
        string name = benchmarks[i];
        set_path(path, name);
        printf("[%lu/%lu: GO ] %s \n", i, num_benchmarks, path);
        bytes_length = read_file_to_buf(bytes, path);
        Options opt = {.disable_memory_bounds = false, .mangle_table_index=false, .dlsym_trim_underscore=true};
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
                printf(
                        "[%lu/%lu: OK ] %s (output: 0x%x = %u, load module: %fs, total: "
                        "%fs)\n",
                        i + 1, num_benchmarks, path, m->stack->value.uint32, m->stack->value.uint32, load,
                        total);
            }
        } else {
            printf("[%lu/%lu:FAIL] %s did not have an exported function " MAIN
                   "\n",
                   i, num_benchmarks, path);
        }
    }
}

int main(int argc, const char *argv[]) {
    string benchmarks[] = {"tak", "fib", "fac", "gcd", "catalan", "primes"};
    run_benchmarks((size_t) (sizeof(benchmarks) / sizeof(string *)), benchmarks);
    return 0;
}
