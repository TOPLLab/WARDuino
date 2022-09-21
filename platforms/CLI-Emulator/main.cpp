//
// WARDuino - WebAssembly interpreter for embedded devices.
//
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

#include "../../src/Debug/debugger.h"
#include "../../src/Utils/macros.h"
#include "../../src/Utils/sockets.h"
#include "../../src/WARDuino.h"
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
    fprintf(stdout, "WARDuino WebAssembly Runtime - 0.2.1\n\n");
    fprintf(stdout, "Usage:\n");
    fprintf(stdout, "    warduino [options] <file>\n");
    fprintf(stdout, "Options:\n");
    fprintf(stdout,
            "    --loop         Let the runtime loop infinitely on exceptions "
            "(default: false)\n");
    fprintf(stdout,
            "    --asserts      Name of file containing asserts to run against "
            "loaded module\n");
    fprintf(stdout,
            "    --watcompiler  Command to compile Wat files to Wasm "
            "binaries (default: wat2wasm)\n");
    fprintf(stdout,
            "    --file         Wasm file (module) to load and execute\n");
    fprintf(stdout,
            "    --no-debug     Run without debug thread"
            "(default: false)\n");
    fprintf(stdout,
            "    --no-socket    Run debug on stdout"
            "(default: false)\n");
    fprintf(stdout,
            "    --socket       Port number for debug socket (ignored if "
            "'--no-socket' is true)"
            "(default: 8192)\n");
    fprintf(stdout,
            "    --paused       Pause program on entry (default: false)\n");
    fprintf(stdout,
            "    --proxy        Localhost port or serial port (ignored if mode "
            "is 'proxy')\n");
    fprintf(stdout,
            "    --mode         The mode to run in: interpreter, proxy "
            "(default: interpreter)\n");
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

void startDebuggerStd(WARDuino *wac, Module *m) {
    int valread;
    uint8_t buffer[1024] = {0};
    wac->debugger->setChannel(fileno(stdout));
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

void startDebuggerSocket(WARDuino *wac, Module *m, int port = 8192) {
    int socket_fd = createSocketFileDescriptor();
    struct sockaddr_in address = createAddress(port);
    bindSocketToAddress(socket_fd, address);
    startListening(socket_fd);
    printf("Listening on port 172.0.0.1:%i\n", port);

    ssize_t valread;
    uint8_t buffer[1024] = {0};
    while (true) {
        int socket = listenForIncomingConnection(socket_fd, address);
        wac->debugger->setChannel(socket);
        while ((valread = read(socket, buffer, 1024)) != -1) {
            write(socket, "got a message ... \n", 19);
            wac->handleInterrupt(valread - 1, buffer);
            write(socket, buffer, valread);
        }
    }
}

// Connect to proxy via a web socket
int connectToProxySocket(int proxy) {
    int channel;
    struct sockaddr_in address = createLocalhostAddress(proxy);

    if ((channel = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        dbg_info("Socket creation error\n");
        return -1;
    }

    if (connect(channel, (struct sockaddr *)&address, sizeof(address)) < 0) {
        dbg_info("Connection failed\n");
        return -1;
    }

    return channel;
}

// Connect to proxy via file descriptor
int connectToProxyFd(const char *proxyfd) { return open(proxyfd, O_RDWR); }

WARDuino *wac = WARDuino::instance();
Module *m;

void *runWAC(void *p) {
    // Print value received as argument:
    dbg_info("\n=== STARTED INTERPRETATION (in separate thread) ===\n");
    wac->run_module(m);
    wac->unload_module(m);
}

int main(int argc, const char *argv[]) {
    ARGV_SHIFT();  // Skip command name

    bool return_exception = true;
    bool run_tests = false;
    bool no_debug = false;
    bool no_socket = false;
    const char *socket = "8192";
    bool paused = false;
    const char *file_name = nullptr;
    const char *proxy = nullptr;
    const char *mode = "interpreter";

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
        } else if (!strcmp("--no-debug", arg)) {
            no_debug = true;
        } else if (!strcmp("--no-socket", arg)) {
            no_socket = true;
        } else if (!strcmp("--socket", arg)) {
            ARGV_GET(socket);
        } else if (!strcmp("--paused", arg)) {
            wac->program_state = WARDUINOpause;
        } else if (!strcmp("--proxy", arg)) {
            ARGV_GET(proxy);  // /dev/ttyUSB0
        } else if (!strcmp("--mode", arg)) {
            ARGV_GET(mode);
        }
    }

    if (argc == 1) {
        ARGV_GET(file_name);
        ARGV_SHIFT();
    }

    if (argc == 0 && file_name != nullptr) {
        if (run_tests) {
            dbg_info("=== STARTING SPEC TESTS ===\n");
            return run_wasm_test(*wac, file_name, asserts_file, watcompiler);
        }
        dbg_info("=== LOAD MODULE INTO WARDUINO ===\n");
        m = load(*wac, file_name,
                 {.disable_memory_bounds = false,
                  .mangle_table_index = false,
                  .dlsym_trim_underscore = false,
                  .return_exception = return_exception});
    } else {
        print_help();
        return 1;
    }

    if (m) {
        m->warduino = wac;

        if (strcmp(mode, "proxy") == 0) {
            // Run in proxy mode
            wac->debugger->proxify();
        } else if (proxy) {
            int connection = -1;

            // Connect to proxy device
            try {
                int port = std::stoi(proxy);
                connection = connectToProxySocket(port);
            } catch (std::invalid_argument const &ex) {
                // argument is not a port
                // treat as filename
                connection = connectToProxyFd(proxy);
            } catch (std::out_of_range const &ex) {
                // argument is an integer but is out of range
                fprintf(stderr,
                        "wdcli: out of range integer argument for --proxy\n");
                return 1;
            }

            if (connection < 0) {
                // Failed to connect stop program
                fprintf(stderr, "wdcli: failed to connect to proxy device\n");
                return 1;
            }

            // Start supervising proxy device (new thread)
            wac->debugger->startProxySupervisor(connection);
        }

        // Run Wasm module (new thread)
        pthread_t id;
        pthread_create(&id, nullptr, runWAC, nullptr);

        // Start debugger
        if (!no_debug) {
            if (no_socket) {
                startDebuggerStd(wac, m);
            } else {
                startDebuggerSocket(wac, m, std::stoi(socket));
            }
        }
        int *ptr;
        pthread_join(id, (void **)&ptr);
    }

    return 0;
}