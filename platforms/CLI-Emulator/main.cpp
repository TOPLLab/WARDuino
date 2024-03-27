//
// WARDuino - WebAssembly interpreter for embedded devices.
//
#include <fcntl.h>
#include <netinet/in.h>
#include <termios.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <thread>

#include "../../src/Debug/debugger.h"
#include "../../src/Utils/macros.h"
#include "warduino/config.h"

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
    fprintf(stdout, "WARDuino WebAssembly Runtime - " PROJECT_VERSION "\n\n");
    fprintf(stdout, "Usage:\n");
    fprintf(stdout, "    wdcli <file> [options]\n");
    fprintf(stdout, "Options:\n");
    fprintf(stdout,
            "    --loop         Let the runtime loop infinitely on exceptions "
            "(default: false)\n");
    fprintf(stdout,
            "    --no-debug     Run without debug thread"
            "(default: false)\n");
    fprintf(stdout,
            "    --no-socket    Run debug on stdout"
            "(default: false)\n");
    fprintf(stdout,
            "    --socket       Port number for debug socket (ignored if "
            "'--no-socket' is true) "
            "(default: 8192)\n");
    fprintf(stdout,
            "    --paused       Pause program on entry (default: false)\n");
    fprintf(stdout,
            "    --proxy        Localhost port or serial port (ignored if mode "
            "is 'proxy')\n");
    fprintf(stdout,
            "    --baudrate     Baudrate to use when connecting to a serial "
            "port (ignored if "
            "no serial port is provided)\n");
    fprintf(stdout,
            "    --mode         The mode to run in: interpreter, proxy "
            "(default: interpreter)\n");
    fprintf(stdout, "    --invoke       Invoke a function from the module\n");
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

void startDebuggerCommunication() {
    Channel *duplex = WARDuino::instance()->debugger->channel;
    if (duplex == nullptr) {
        return;
    }

    duplex->open();

    ssize_t valread;
    uint8_t buffer[1024] = {0};
    while (true) {
        while ((valread = duplex->read(buffer, 1024)) != -1) {
            WARDuino::instance()->handleInterrupt(valread, buffer);
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

struct debugger_options {
    int socket;
    bool no_socket;
};

void setupDebuggerCommunication(debugger_options &options) {
    dbg_info("\n=== STARTED DEBUGGER (in separate thread) ===\n");
    // Start debugger
    Channel *duplex;
    if (options.no_socket) {
        duplex = new Duplex(stdin, stdout);
    } else {
        duplex = new WebSocket(options.socket);
    }

    wac->debugger->setChannel(duplex);
}

const std::map<std::string, speed_t> &baudrateMap() {
    static const auto *map = new std::map<std::string, speed_t>{
        {"0", B0},           {"50", B50},         {"75", B75},
        {"110", B110},       {"134", B134},       {"150", B150},
        {"200", B200},       {"300", B300},       {"600", B600},
        {"1200", B1200},     {"1800", B1800},     {"2400", B2400},
        {"4800", B4800},     {"9600", B9600},     {"19200", B19200},
        {"38400", B38400},   {"38400", B38400},   {"57600", B57600},
        {"115200", B115200}, {"230400", B230400},
    };
    return *map;
}

bool configureSerialPort(int serialPort, const char *argument) {
    struct termios tty {};
    if (tcgetattr(serialPort, &tty) != 0) {
        fprintf(stderr, "wdcli: error configuring serial port (errno %i): %s\n",
                errno, strerror(errno));
        return false;
    }

    tty.c_cflag &= ~PARENB;  // Disable parity bit
    tty.c_cflag &= ~CSTOPB;  // Disable stop field

    tty.c_cflag &= ~CSIZE;  // Byte is 8 bits
    tty.c_cflag |= CS8;

    tty.c_cflag &= ~CRTSCTS;  // Disable RTS/CTS
    tty.c_cflag |=
        CREAD | CLOCAL;  // Turn on READ & ignore ctrl lines (CLOCAL= 1)
    tty.c_lflag &= ~ICANON;
    tty.c_lflag &= ~ECHO;    // No echo
    tty.c_lflag &= ~ECHOE;   // No erasure
    tty.c_lflag &= ~ECHONL;  // No new-line echo
    tty.c_lflag &= ~ISIG;
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR |
                     ICRNL);  // No special handling
    tty.c_oflag &= ~OPOST;    // No output bytes interpretation
    tty.c_oflag &= ~ONLCR;    // No carriage return conversion
    tty.c_cc[VTIME] = 1;      // Wait max 1sec
    tty.c_cc[VMIN] = 0;

    auto iterator = baudrateMap().find(argument);
    if (iterator == baudrateMap().end()) {
        fprintf(stderr, "Provided argument %s is unsupported\n", argument);
        return false;
    }
    speed_t baudrate = iterator->second;

    cfsetispeed(&tty, baudrate);
    cfsetospeed(&tty, baudrate);

    if (tcsetattr(serialPort, TCSANOW, &tty) != 0) {
        fprintf(stderr, "Error %i from tcsetattr: %s\n", errno,
                strerror(errno));
        return false;
    }
    return true;
}

StackValue parseParameter(const char *input, uint8_t value_type) {
    StackValue value = {value_type, {0}};
    switch (value_type) {
        case I32: {
            value.value.uint32 = std::stoi(input);
            break;
        }
        case F32: {
            value.value.f32 = std::atof(input);
            break;
        }
        case I64: {
            value.value.uint64 = std::stoi(input);
            break;
        }
        case F64: {
            value.value.f64 = std::atof(input);
            break;
        }
        default:
            FATAL("wdcli: '%s' should be of type %hhu\n", input, value_type);
    }
    return value;
}

int main(int argc, const char *argv[]) {
    ARGV_SHIFT();  // Skip command name

    bool return_exception = true;
    bool no_debug = false;
    bool no_socket = false;
    const char *socket = "8192";
    bool initiallyPaused = false;
    const char *file_name = nullptr;
    const char *proxy = nullptr;
    const char *baudrate = nullptr;
    const char *mode = "interpreter";

    const char *fname = nullptr;
    std::vector<StackValue> arguments = std::vector<StackValue>();

    if (argc > 0 && argv[0][0] != '-') {
        ARGV_GET(file_name);

        dbg_info("=== LOAD MODULE INTO WARDUINO ===\n");
        m = load(*wac, file_name,
                 {.disable_memory_bounds = false,
                  .mangle_table_index = false,
                  .dlsym_trim_underscore = false,
                  .return_exception = return_exception});
    }

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
        } else if (!strcmp("--no-debug", arg)) {
            no_debug = true;
        } else if (!strcmp("--no-socket", arg)) {
            no_socket = true;
        } else if (!strcmp("--socket", arg)) {
            ARGV_GET(socket);
        } else if (!strcmp("--paused", arg)) {
            initiallyPaused = true;
        } else if (!strcmp("--proxy", arg)) {
            ARGV_GET(proxy);  // /dev/ttyUSB0
        } else if (!strcmp("--baudrate", arg)) {
            ARGV_GET(baudrate);
        } else if (!strcmp("--mode", arg)) {
            ARGV_GET(mode);
        } else if (!strcmp("--invoke", arg)) {
            ARGV_GET(fname);

            // find function
            int fidx = wac->get_export_fidx(m, fname);
            if (fidx < 0) {
                fprintf(stderr, "wdcli: no exported function with name '%s'\n",
                        fname);
                return 1;
            }

            Block function = m->functions[fidx];

            // consume all arguments for the function
            for (uint32_t i = 0; i < function.type->param_count; ++i) {
                const char *number = nullptr;
                ARGV_GET(number);

                if (number[0] == '-') {
                    FATAL("wdcli: wrong number of arguments for '%s'\n", fname);
                }

                arguments.push_back(
                    parseParameter(number, function.type->params[i]));
            }
        }
    }

    if (argc != 0 || file_name == nullptr) {
        print_help();
        return 1;
    }

    m->warduino = wac;

    if (initiallyPaused) {
        wac->debugger->pauseRuntime(m);
    }

    if (m) {
        if (strcmp(mode, "proxy") == 0) {
            // Run in proxy mode
            wac->debugger->proxify();
        } else if (proxy) {
            // Connect to proxy device
            Channel *connection = nullptr;
            try {
                int port = std::stoi(proxy);
                connection = new ClientSocket(port);
            } catch (std::invalid_argument const &ex) {
                // argument is not a port
                // treat as filename
                int serialPort = open(proxy, O_RDWR);
                if (serialPort < 0) {
                    fprintf(stderr, "wdcli: error opening %s: %s\n", proxy,
                            strerror(errno));
                    return 1;
                }
                if (baudrate == nullptr) {
                    fprintf(stderr, "wdcli: baudrate not specified\n");
                    return 1;
                }

                if (!configureSerialPort(serialPort, baudrate)) {
                    return 1;
                }
                connection = new FileDescriptorChannel(serialPort);
            } catch (std::out_of_range const &ex) {
                // argument is an integer but is out of range
                fprintf(stderr,
                        "wdcli: out of range integer argument for --proxy\n");
                return 1;
            }

            if (connection == nullptr) {
                // Failed to connect stop program
                fprintf(stderr, "wdcli: failed to connect to proxy device\n");
                return 1;
            }

            // Start supervising proxy device (new thread)
            wac->debugger->startProxySupervisor(connection);
        }

        // Start debugger (new thread)
        std::thread communication;
        if (!no_debug) {
            debugger_options options = debugger_options();
            options.no_socket = no_socket;
            options.socket = std::stoi(socket);
            setupDebuggerCommunication(options);

            communication = std::thread(startDebuggerCommunication);
        }

        // Run Wasm module
        dbg_info("\n=== STARTED INTERPRETATION (main thread) ===\n");
        if (fname != nullptr) {
            uint32_t fidx = wac->get_export_fidx(m, fname);
            wac->invoke(m, fidx, arguments.size(), &arguments[0]);
        } else {
            wac->run_module(m);
        }
        wac->unload_module(m);
        wac->debugger->stop();

        if (!no_debug) {
            communication.join();
        }
    }

    return 0;
}
