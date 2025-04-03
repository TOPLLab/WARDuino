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
#include <utility>
#include <fstream>
#include <iostream>

#include "../../src/Debug/debugger.h"
#include "../../src/Interpreter/concolic_interpreter.h"
#include "../../src/Utils/macros.h"
#include "../../src/Utils/util.h"
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
    fprintf(stdout, "    warduino <file> [options]\n");
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
            "'--no-socket' is true)"
            "(default: 8192)\n");
    fprintf(stdout,
            "    --paused       Pause program on entry (default: false)\n");
    fprintf(stdout,
            "    --proxy        Localhost port or serial port (ignored if mode "
            "is 'proxy')\n");
    fprintf(stdout,
            "    --baudrate        Baudrate to use when connecting to a serial "
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
    const char *socket;
    bool no_socket;
};

void setupDebuggerCommunication(debugger_options *options) {
    dbg_info("\n=== STARTED DEBUGGER (in separate thread) ===\n");
    // Start debugger
    Channel *duplex;
    if (options->no_socket) {
        duplex = new Duplex(stdin, stdout);
    } else {
        int port = std::stoi(options->socket);
        duplex = new WebSocket(port);
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

void load_snapshot(const std::vector<std::string> &snapshot_messages) {
    std::cout << "Loading snapshot data:" << std::endl;
    for (const std::string &msg : snapshot_messages) {
        std::cout << "Adding debug message: \"" << msg << "\"" << std::endl;
        wac->debugger->addDebugMessage(msg.length() + 1,
                                       (uint8_t *)(msg + "\n").c_str());
    }
    while (wac->debugger->checkDebugMessages(m, &wac->program_state)) {
    }
    wac->program_state = WARDUINOrun;
}

z3::expr simplify_custom(z3::expr expression) {
    if (expression.is_app()) {
        z3::func_decl decl = expression.decl();
        if (decl.decl_kind() == Z3_OP_EQ) {
            if (expression.arg(0).decl().decl_kind() == Z3_OP_ITE) {
                z3::expr l = expression.arg(0).arg(1);
                z3::expr r = expression.arg(1);
                if (l.is_const() && r.is_const() && l.get_numeral_int() == r.get_numeral_int()) {
                    return expression.arg(0).arg(0);
                }
            }
            return expression;
        }
        else if (decl.decl_kind() == Z3_OP_ITE) {
        }
        return expression;
    }
    return expression;
}

void z3_pretty_print(z3::expr expression) {
    expression = simplify_custom(expression);
    if (expression.is_app()) {
        z3::func_decl decl = expression.decl();

        if (decl.decl_kind() == Z3_OP_SLT) {
            std::cout << "(";
            z3_pretty_print(expression.arg(0));
            std::cout << " < ";
            z3_pretty_print(expression.arg(1));
            std::cout << ")";
            return;
        }
        if (decl.decl_kind() == Z3_OP_SLEQ) {
            std::cout << "(";
            z3_pretty_print(expression.arg(0));
            std::cout << " <= ";
            z3_pretty_print(expression.arg(1));
            std::cout << ")";
            return;
        }
        else if (decl.decl_kind() == Z3_OP_EQ) {
            std::cout << "(";
            z3_pretty_print(expression.arg(0));
            std::cout << " == ";
            z3_pretty_print(expression.arg(1));
            std::cout << ")";
            return;
        }
        else if (decl.decl_kind() == Z3_OP_ITE) {
            std::cout << "(";
            z3_pretty_print(expression.arg(0));
            std::cout << " ? ";
            z3_pretty_print(expression.arg(1));
            std::cout << " : ";
            z3_pretty_print(expression.arg(2));
            std::cout << ")";
            return;
        }
        else if (decl.decl_kind() == Z3_OP_AND) {
            std::cout << "(";
            z3_pretty_print(expression.arg(0));
            std::cout << " && ";
            z3_pretty_print(expression.arg(1));
            std::cout << ")";
            return;
        }
        else if (decl.decl_kind() == Z3_OP_NOT) {
            std::cout << "!";
            z3_pretty_print(expression.arg(0));
            return;
        }
        else if (decl.decl_kind() == Z3_OP_BNUM) {
            std::cout << expression.get_decimal_string(3);
            return;
        }
        /*else {
            std::cout << decl.name();
        }

        int arg_count = expression.num_args();
        std::cout << "(";
        for (int i = 0; i < arg_count; i++) {
            if (i != 0) {
                std::cout << ", ";
            }
            std::cout << expression.arg(i);
        }
        std::cout << ")";
        std::cout << std::endl;*/
        //std::cout << decl.decl_kind();
        std::cout << expression;
    }
    else {
        std::cout << expression;
    }
}

void z3_pretty_println(z3::expr expression) {
    z3_pretty_print(expression);
    std::cout << std::endl;
}

struct Model {
    std::unordered_map<std::string, SymbolicValueMapping> values;
    z3::expr path_condition;
    std::vector<Model> subpaths;

    Model(const std::unordered_map<std::string, SymbolicValueMapping> &values,
          z3::expr path_condition)
        : values(values), path_condition(std::move(path_condition)) {
        //subpaths.push_back(*this);
    }

    z3::expr x_only_path_condition(size_t index) {
        z3::expr_vector from(m->ctx);
        z3::expr_vector to(m->ctx);
        // TODO: values.size() maybe dangerous with substitute if it's higher than the current iteration symbolic variable count
        for (size_t i = 0; i < values.size(); i++) {
            if (i == index) continue;
            std::string var_name = "x_" + std::to_string(i);
            from.push_back(m->ctx.bv_const(var_name.c_str(), 32));
            to.push_back(m->ctx.bv_val(values[var_name].concrete_value.value.uint64, 32));
        }
        return path_condition.substitute(from, to).simplify();
    }

    void add_partial_match(Model em, size_t depth) {
        if (depth >= em.values.size())
            return;

        z3::expr x_only_path_condition = em.x_only_path_condition(depth);
        std::cout << x_only_path_condition << std::endl;
        std::string sym_var_name = "x_" + std::to_string(depth);
        auto already_exists = std::find_if(
            subpaths.begin(), subpaths.end(), [x_only_path_condition, this, sym_var_name, depth](Model otherModel) {
                std::cout << "Compare with " << otherModel.x_only_path_condition(depth) << std::endl;
                z3::solver s(m->ctx);
                s.add(z3::forall(m->ctx.bv_const(sym_var_name.c_str(), 32),
                                 otherModel.x_only_path_condition(depth) ==
                                     x_only_path_condition));
                return s.check() == z3::sat;
            });
        // Doesn't exist!
        if (already_exists == subpaths.end()) {
            std::cout << sym_var_name << std::endl;
            std::cout << "New:" << std::endl;
            std::cout << em.values[sym_var_name].concrete_value.value.uint64 << std::endl;
            std::cout << "Existing:" << std::endl;
            for (Model otherModel : subpaths) {
                std::cout << otherModel.values[sym_var_name].concrete_value.value.uint64 << std::endl;
            }
            std::cout << "New subpath!" << std::endl;
            em.add_partial_match(em, depth + 1);
            subpaths.push_back(em);
        }
        else {
            Model &m = *already_exists;
            m.add_partial_match(em, depth + 1);
        }
    }

    std::string to_string(size_t depth) {
        std::string str = "";
        if (depth == 0) {
            str += "*\n";
        }
        for (Model path : subpaths) {
            for (size_t i = 0; i < depth + 1; i++) {
                str += "\t";
            }
            str += std::to_string(path.values["x_" + std::to_string(depth)].concrete_value.value.int32);
            str += "\n";
            str += path.to_string(depth + 1);
        }
        return str;
    }

    int count_leaf_nodes() {
        if (subpaths.empty()) {
            return 1;
        }
        int count = 0;
        for (Model path : subpaths) {
            count += path.count_leaf_nodes();
        }
        return count;
    }

    nlohmann::json to_json() {
        nlohmann::json graph;
        graph["paths"] = to_json(0);
        return graph;
    }

    nlohmann::json to_json(size_t depth) {
        auto paths = std::vector<nlohmann::json>();
        for (Model path : subpaths) {
            nlohmann::json path_node;
            SymbolicValueMapping value = path.values["x_" + std::to_string(depth)];
            path_node["value"] = value.concrete_value.value.int32;
            path_node["primitive"] = value.primitive_origin;
            path_node["arg"] = value.primitive_argument;
            path_node["time_step"] = value.time_step;
            path_node["paths"] = path.to_json(depth + 1);
            paths.push_back(path_node);
        }
        return paths;
    }
};

void run_concolic(const std::vector<std::string>& snapshot_messages, int max_instructions = 50, int max_sym_vars = -1) {
    wac->interpreter = new ConcolicInterpreter();
    // Has a big impact on performance, for example if you have a simple program
    // with a loop that contains an if statement and, you run the loop 30 times
    // then you have 2^30 possible branching paths. You can take the if branch
    // in the first loop, not take it in the second, and so on.
    //wac->max_instructions = -1;
    //wac->max_instructions = 900;
    wac->max_instructions = max_instructions;
    wac->max_symbolic_variables = max_sym_vars;
    int total_instructions_executed = 0;

    z3::expr global_condition = m->ctx.bool_val(true);
    int iteration_index = 0;
    std::vector<std::unordered_map<std::string, SymbolicValueMapping>> models;
    Model graph = Model({}, m->ctx.bool_val(true));
    while (true) {
        std::cout << std::endl
                  << "=== CONCOLIC ITERATION " << iteration_index
                  << " ===" << std::endl;
        m->symbolic_variable_count = 0;
        m->path_condition = m->ctx.bool_val(true);
        m->instructions_executed = 0;

        bool success;
        if (!snapshot_messages.empty()) {
            load_snapshot(snapshot_messages);
            // Remove any breakpoints that might have been set, we don't want the concolic execution to pause on them.
            wac->debugger->breakpoints.clear();
            m->create_symbolic_state();
            success = wac->interpreter->interpret(m);
        } else {
            success = wac->run_module(m);
        }

        if (!success) {
            std::cout << "Trap: " << m->exception << std::endl;
            std::cout << "Model that caused issue:" << std::endl;
            for (const auto &entry : m->symbolic_concrete_values) {
                std::cout << "  " << entry.first << " = "
                          << entry.second.concrete_value.value.int32 << std::endl;
            }
            break;
        }

        if (iteration_index == 0) {
            z3::solver s(m->ctx);
            s.add(m->path_condition);
            auto result = s.check();
            assert(result == z3::sat);
            std::cout << "Iteration 0, fixing default values" << std::endl;
            std::cout << "Model:" << std::endl;
            z3::model model = s.get_model();
            for (int i = 0; i < (int)model.size(); i++) {
                z3::func_decl func = model[i];
                std::cout << func.name() << " = "
                          << model.get_const_interp(func) << std::endl;
                m->symbolic_concrete_values[func.name().str()]
                    .concrete_value.value.uint64 =
                    model.get_const_interp(func).get_numeral_uint64();
            }
        }
        iteration_index++;
        models.push_back(m->symbolic_concrete_values);

        graph.add_partial_match(Model(m->symbolic_concrete_values, m->path_condition), 0);

        // Start a new concolic iteration by solving !path_condition.
        // TODO: When should I use simplify? Does the solver automatically
        // simplify things so I can just let it handle that? Maybe I should
        // only use simplify when building up expressions during symbolic
        // execution?
        /*std::cout << "Execution finished, path condition = "
                  << m->path_condition.simplify() << std::endl;
        std::cout << "PC = ";
        z3_pretty_print(m->path_condition);
        std::cout << std::endl;*/
        z3::solver s(m->ctx);
        /*std::cout << "!path_condition = " << !m->path_condition
                  << std::endl;
        std::cout << "!path_condition (simplified) = "
                  << (!m->path_condition).simplify() << std::endl;*/
        // s.add(!m->path_condition);
        std::cout << m->path_condition.simplify() << std::endl;
        m->path_condition = m->path_condition.simplify();
        global_condition = global_condition &&
                           !m->path_condition;  // Not this path and also
                                                // not the previous paths

        /*std::cout << "GPC = ";
        z3_pretty_print(global_condition);
        std::cout << std::endl;
        std::cout << global_condition << std::endl;*/
        total_instructions_executed += m->instructions_executed;
        s.add(global_condition);
        if (s.check() == z3::unsat) {
            std::cout << "Explored all paths!" << std::endl;
            break;
        }
        /*std::cout << "Solve !path_condition:" << std::endl
                  << s.get_model() << std::endl;*/

        z3::model model = s.get_model();
        //std::cout << "Model:" << std::endl;
        /*std::vector<Z3_ast> from;
        std::vector<Z3_ast> to;*/
        for (int i = 0; i < (int)model.size(); i++) {
            z3::func_decl func = model[i];
            /*std::cout << func.name() << " = "
                      << model.get_const_interp(func) << std::endl;*/
            m->symbolic_concrete_values[func.name().str()].concrete_value.value.uint64 =
                model.get_const_interp(func).get_numeral_uint64();

            /*if (func.name().str() != "x_0") {
                from.push_back(m->ctx.bv_const(func.name().str().c_str(), 32));
                to.push_back(model.get_const_interp(func));
            }*/
        }
        /*std::cout << "x_0 only path condition: " << std::endl;
        z3_pretty_println(z3::to_expr(m->ctx, Z3_substitute(m->ctx, m->path_condition, from.size(),from.data(),to.data())));
        z3_pretty_println(m->path_condition);*/
    }

    std::cout << std::endl << "=== FINISHED ===" << std::endl;
    /*std::cout << "Models found:" << std::endl;
    for (size_t i = 0; i < models.size(); i++) {
        std::cout << "- Model #" << i << ":" << std::endl;
        for (const auto &entry : models[i]) {
            std::cout << "  " << entry.first << " = "
                      << entry.second.value.int32 << std::endl;
        }
    }*/

    std::cout << "Total amount of instructions executed: " << total_instructions_executed << std::endl;
    std::cout << "Models found:" << std::endl;
    std::cout << graph.to_string(0) << std::endl;
    std::cout << graph.to_json() << std::endl;
    std::cout << "Found " << graph.count_leaf_nodes() << " uniqe paths!" << std::endl;
    /*for (size_t i = 0; i < x0_models.size(); i++) {
        std::cout << "- Model #" << i << ":" << std::endl;
        //z3_pretty_println(x0_models[i].path_condition);
        for (const auto &entry : x0_models[i].values) {
            std::cout << "  " << entry.first << " = "
                      << entry.second.concrete_value.value.int32 << std::endl;
        }
    }

    nlohmann::json json_models;
    json_models["models"] = std::vector<nlohmann::json>();
    for (auto &model : x0_models) {
        nlohmann::json j;
        for (const auto &entry : model.values) {
            nlohmann::json value;
            value["primitive"] = entry.second.primitive_origin;
            value["arg"] = entry.second.primitive_argument;
            value["value"] = entry.second.concrete_value.value.int32;
            j[entry.first] = value;
        }
        json_models["models"].push_back(j);
    }
    std::cout << json_models << std::endl;*/
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
    const char *max_instructions_str = "50";
    const char *max_symbolic_variables_str = "-1";
    bool dump_info = false;

    const char *fname = nullptr;
    std::vector<StackValue> arguments = std::vector<StackValue>();

    const char *current_msg = nullptr;
    std::vector<std::string> snapshot_messages;

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

            Block *function = m->functions[fidx];

            // consume all arguments for the function
            for (uint32_t i = 0; i < function->type->param_count; ++i) {
                const char *number = nullptr;
                ARGV_GET(number);

                if (number[0] == '-') {
                    FATAL("wdcli: wrong number of arguments for '%s'\n", fname);
                }

                arguments.push_back(
                    parseParameter(number, function->type->params[i]));
            }
        } else if (!strcmp("--snapshot", arg)) {
            ARGV_GET(current_msg);

            if (std::filesystem::exists(current_msg)) {
                std::ifstream ifs(current_msg);
                std::string line;
                while (std::getline(ifs, line)) {
                    snapshot_messages.emplace_back(line);
                }
            }
            else {
                while (strcmp("end", current_msg) != 0) {
                    snapshot_messages.emplace_back(current_msg);
                    ARGV_GET(current_msg);
                }
            }
        } else if (!strcmp("--max-instructions", arg)) {
            ARGV_GET(max_instructions_str);
        } else if (!strcmp("--max-symbolic-variables", arg)) {
            ARGV_GET(max_symbolic_variables_str);
        } else if (!strcmp("--dump-info", arg)) {
            dump_info = true;
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
        if (dump_info) {
            auto choicepoints = std::vector<uint32_t>();
            for (uint8_t *choice_point : m->find_choice_points()) {
                choicepoints.push_back(toVirtualAddress(choice_point, m));
            }
            auto after_choicepoints = std::vector<uint32_t>();
            for (uint8_t *choice_point : m->find_choice_points(true)) {
                after_choicepoints.push_back(toVirtualAddress(choice_point, m));
            }
            auto primitive_calls = std::vector<uint32_t>();
            for (uint8_t *call_site : m->find_pc_before_primitive_calls()) {
                primitive_calls.push_back(toVirtualAddress(call_site, m));
            }
            auto after_primitive_calls = std::vector<uint32_t>();
            for (uint8_t *call_site : m->find_pc_after_primitive_calls()) {
                after_primitive_calls.push_back(toVirtualAddress(call_site, m));
            }
            nlohmann::json json;
            json["choicepoints"] = choicepoints;
            json["after_choicepoints"] = after_choicepoints;
            json["primitive_calls"] = primitive_calls;
            json["after_primitive_calls"] = after_primitive_calls;

            std::vector<std::string> fidx_mapping = std::vector<std::string>();
            for (uint32_t fidx = 0; fidx < m->import_count; fidx++) {
                fidx_mapping.emplace_back(m->functions[fidx]->import_field);
            }
            json["primitive_fidx_mapping"] = fidx_mapping;

            std::cout << json << std::endl;
            wac->unload_module(m);
            exit(0);
        }

        if (strcmp(mode, "proxy") == 0) {
            // Run in proxy mode
            wac->debugger->proxify();
        } else if (proxy) {
            // Connect to proxy device
            Channel *connection = nullptr;
            try {
                int port = std::stoi(proxy);
                connection = new WebSocket(port);
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
            auto *options =
                new debugger_options();
            options->no_socket = no_socket;
            options->socket = socket;
            setupDebuggerCommunication(options);
            delete options;

            communication = std::thread(startDebuggerCommunication);
        }

        // Run Wasm module
        if (strcmp(mode, "concolic") == 0) {
            run_concolic(snapshot_messages, std::stoi(max_instructions_str), std::stoi(max_symbolic_variables_str));
        }
        else {
            // TODO: Add option to calculate the choice points and add them as breakpoints from the remote debugger once
            // the user starts debugging instead of always adding them even when not debugging.
            for (uint8_t *choice_point : m->find_choice_points()) {
                //wac->debugger->addBreakpoint(choice_point);
            }

            dbg_info("\n=== STARTED INTERPRETATION (main thread) ===\n");
            if (fname != nullptr) {
                uint32_t fidx = wac->get_export_fidx(m, fname);
                wac->invoke(m, fidx, arguments.size(), &arguments[0]);
            } else {
                wac->run_module(m);
            }
        }

        wac->unload_module(m);
        wac->debugger->stop();
        Z3_finalize_memory();

        if (communication.joinable()) {
            communication.join();
        }
    }

    return 0;
}
