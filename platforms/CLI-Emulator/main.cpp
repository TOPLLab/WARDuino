//
// WARDuino - WebAssembly interpreter for embedded devices.
//
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../../src/Debug/debugger.h"
#include "../../src/Utils/macros.h"
#include "../../tests/integration/wasm_tests.h"

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
    fprintf(stdout, "WARDuino WebAssembly Runtime - 0.1.0\n\n");
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
    fprintf(stdout,
            "    --no-socket    Run without socket "
            "(default: false)\n");
}

Module *load(WARDuino wac, const char *file_name, Options opt) {
    uint8_t *wasm;
    unsigned int file_size;

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

// Socket Debugger Interface
void setFileDescriptorOptions(int socket_fd) {
    int opt = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("Failed to set socket file descriptor options");
        exit(EXIT_FAILURE);
    }
}

int createSocketFileDescriptor() {
    int socket_fd;
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Failed to make a new socket file descriptor");
        exit(EXIT_FAILURE);
    }
    setFileDescriptorOptions(socket_fd);
    return socket_fd;
}

void bindSocketToAddress(int socket_fd, struct sockaddr_in address) {
    if (bind(socket_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Binding socket to address failed");
        exit(EXIT_FAILURE);
    }
}

struct sockaddr_in createAddress(int port) {
    struct sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    return address;
}

void startListening(int socket_fd) {
    if (listen(socket_fd, 1) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
}

int listenForIncomingConnection(int socket_fd, struct sockaddr_in address) {
    int new_socket;
    int size = sizeof(address);
    if ((new_socket = accept(socket_fd, (struct sockaddr *)&address,
                             (socklen_t *)&size)) < 0) {
        perror("Failed to listen for incoming connections");
        exit(EXIT_FAILURE);
    }
    return new_socket;
}

void startDebuggerStd(WARDuino *wac, Module *m) {
    int valread;
    uint8_t buffer[1024] = {0};
    wac->debugger->socket = fileno(stdout);
    while (true) {
        debug("waiting for debug command\n");
        while ((valread = read(fileno(stdin), buffer, 1024)) != -1) {
            write(fileno(stdout), "got a message ... \n", 19);
            wac->handleInterrupt(valread - 1, buffer);
            write(fileno(stdout), buffer, valread);
            fflush(stdout);
        }
    }
}

void startDebuggerSocket(WARDuino *wac, Module *m) {
    int socket_fd = createSocketFileDescriptor();
    struct sockaddr_in address = createAddress(8192);
    bindSocketToAddress(socket_fd, address);
    startListening(socket_fd);

    int valread;
    uint8_t buffer[1024] = {0};
    while (true) {
        int socket = listenForIncomingConnection(socket_fd, address);
        wac->debugger->socket = socket;
        //        wac->debugger->socket = fileno(stdout); // todo remove
        while ((valread = read(socket, buffer, 1024)) != -1) {
            write(socket, "got a message ... \n", 19);
            wac->handleInterrupt(valread - 1, buffer);
            // runningstate program_state = warduinorun;
            write(socket, buffer, valread);
            // while (checkdebugmessages(m, &program_state)) {
            //				printf("checkdebugmessages \n");
            //};
            // fflush(stdout);
        }
    }
}

WARDuino wac;
Module *m;

void *runWAC(void *p) {
    // Print value received as argument:
    dbg_info("\n=== STARTED INTERPRETATION (in separate thread) ===\n");
    wac.run_module(m);
    wac.unload_module(m);
}

int main(int argc, const char *argv[]) {
    ARGV_SHIFT();  // Skip command name

    bool return_exception = true;
    bool run_tests = false;
    bool no_socket = false;
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
        } else if (!strcmp("--no-socket", arg)) {
            no_socket = true;
        }
    }

    if (argc == 1) {
        ARGV_GET(file_name);
        ARGV_SHIFT();
    }

    if (argc == 0 && file_name != nullptr) {
        if (run_tests) {
            dbg_info("=== STARTING SPEC TESTS ===\n");
            return run_wasm_test(wac, file_name, asserts_file, watcompiler);
        }
        dbg_info("=== LOAD MODULE INTO WARDUINO ===\n");
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
        pthread_t id;
        uint8_t command[] = {'0', '3', '\n'};
        // wac.handleInterrupt(3, command);
        m->warduino = &wac;
        pthread_create(&id, nullptr, runWAC, nullptr);
        if (no_socket) {
            startDebuggerStd(&wac, m);
        } else {
            startDebuggerSocket(&wac, m);
        }
        int *ptr;
        pthread_join(id, (void **)&ptr);
    }

    return 0;
}
