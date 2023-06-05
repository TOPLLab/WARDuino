#include "interruptfixture.h"

InterruptFixture::InterruptFixture(const char* t_interruptName, uint8_t* t_wasm,
                                   size_t t_wasm_len)
    : wasm(t_wasm),
      wasm_len(t_wasm_len),
      warduino(WARDuino::instance()),
      interruptName(t_interruptName) {}

InterruptFixture::~InterruptFixture() {}

void InterruptFixture::SetUp() {
    Options opts = {.disable_memory_bounds = false,
                    .mangle_table_index = false,
                    .dlsym_trim_underscore = false,
                    .return_exception = true};
    this->wasm_module = warduino->load_module(wasm, wasm_len, opts);
    this->dbgOutput = new DBGOutput(warduino->debugger);
    if (!this->dbgOutput->open()) {
        FAIL() << "could not open connect tmpfile";
    }

    this->debugger = warduino->debugger;
    callstackBuilder = new CallstackBuilder(this->wasm_module);
    moduleCompanion = new ModuleCompanion(this->wasm_module);
}

void InterruptFixture::failAndPrintAllReceivedMessages(const char* failReason) {
    std::string errorMsg{};
    this->dbgOutput->appendReadLines(&errorMsg);
    FAIL() << "Valid Answer for " << this->interruptName
           << " was not received. Received following "
              "lines:\n"
           << errorMsg;
}

void InterruptFixture::TearDown() {
    warduino->unload_module(wasm_module);
    wasm_module = nullptr;
    debugger->breakpoints.clear();
    debugger = nullptr;
    delete this->dbgOutput;
    delete callstackBuilder;
    delete moduleCompanion;
}