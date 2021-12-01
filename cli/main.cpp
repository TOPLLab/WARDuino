//
// WARDuino - WebAssembly interpreter for embedded devices.
//
#include <cstdio>
#include <cstring>

#include "../WARDuino.h"
#include "../tests/integration/wasm_tests.h"

// Constants
#define MAX_MODULE_SIZE (64 * 1024 * 1024)

#define ARGV_SHIFT() \
    {                \
        argc--;      \
        argv++;      \
    }
#define ARGV_GET(x)        \
    {                      \
        if (argc > 0) {    \
            (x) = argv[0]; \
            ARGV_SHIFT();  \
        }                  \
    }

void print_help() {
    //    fprintf(stdout, "WARDuino WebAssembly Runtime - 0.1.0\n\n");
    fprintf(stdout, "Usage:\n");
    fprintf(stdout, "    warduino [options] <file>\n");
    fprintf(stdout, "Options:\n");
    fprintf(
        stdout,
        "    --loop         Let the runtime loop infinitely on exceptions\n");
    fprintf(stdout,
            "    --asserts      Name of file containing asserts to run against "
            "loaded module\n");
    fprintf(stdout,
            "    --watcompiler  Command to compile Wat files to Wasm "
            "binaries (default: wat2wasm)\n");
    fprintf(stdout,
            "    --file         Wasm file (module) to load and execute\n");
}

Module *load(WARDuino wac, const char *file_name, Options opt) {
    uint8_t *wasm;
    uint file_size;

    FILE *file = fopen(file_name, "rb");
    if (!file) {
        fprintf(stderr, "cannot open file");
        exit(1);
    }

    fseek(file, 0, SEEK_END);
    file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size > MAX_MODULE_SIZE) {
        fprintf(stderr, "file is too large");
        goto error;
    }

    wasm = (uint8_t *)malloc(file_size);
    if (!wasm) {
        fprintf(stderr, "not enough memory for wasm binary");
        goto error;
    }

    if (fread(wasm, 1, file_size, file) != file_size) {
        fprintf(stderr, "could not read file");
        goto error;
    }
    fclose(file);
    file = nullptr;

    return wac.load_module(wasm, file_size, opt);

error:
    fclose(file);

    return nullptr;
}

int main(int argc, const char *argv[]) {
    ARGV_SHIFT();  // Skip command name

    WARDuino wac;
    Module *m;
    bool return_exception = true;
    bool run_tests = false;
    const char *file_name = nullptr;

    const char *asserts_file = nullptr;
    const char *watcompiler = "wat2wasm";

    // Parse options
    while (argc > 0) {
        const char *arg = argv[0];
        if (arg[0] != '-') {
            break;
        }

        ARGV_SHIFT();
        if (!strcmp("--help", arg)) {
            print_help();
            return 0;
        } else if (!strcmp("--loop", arg)) {
            return_exception = false;
        } else if (!strcmp("--file", arg)) {
            ARGV_GET(file_name);
        } else if (!strcmp("--asserts", arg)) {
            run_tests = true;
            ARGV_GET(asserts_file);
        } else if (!strcmp("--watcompiler", arg)) {
            ARGV_GET(watcompiler);
        }
    }

    if (argc == 1) {
        ARGV_GET(file_name);
        ARGV_SHIFT();
    }

    if (argc == 0 && file_name != nullptr) {
        if (run_tests) {
            run_wasm_test(wac, file_name, asserts_file, watcompiler);
            return 0;
        }
        m = load(wac, file_name,
                 {.disable_memory_bounds = false,
                  .mangle_table_index = false,
                  .dlsym_trim_underscore = false,
                  .return_exception = return_exception});
    } else {
        print_help();
        return 1;
    }

    if (m) {
        wac.run_module(m);
        wac.unload_module(m);
    }

    return 0;
}