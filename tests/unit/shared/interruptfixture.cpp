#include "interruptfixture.h"

#include "../../../src/Utils/util.h"

InterruptFixture::InterruptFixture(const char* t_interruptName,
                                   uint8_t t_interruptNr, uint8_t* t_wasm,
                                   size_t t_wasm_len)
    : wasm(t_wasm),
      wasm_len(t_wasm_len),
      warduino(WARDuino::instance()),
      interruptName(t_interruptName),
      interruptNr(t_interruptNr) {}

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

std::string InterruptFixture::fullErrorMessage(const char* msg) {
    std::string errorMsg{msg};
    errorMsg += "\nReceived following lines:\n";
    this->dbgOutput->appendReadLines(&errorMsg);
    return errorMsg;
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

// creates an interruptMsg that does not expect any payload
void InterruptFixture::sendInterruptNoPayload(uint8_t interruptNr) {
    char hexa[3] = {};
    chars_as_hexa((unsigned char*)hexa, &interruptNr, 1);
    hexa[2] = '\n';

    const uint8_t* content = (uint8_t*)hexa;
    this->debugger->addDebugMessage(3, content);
    this->debugger->checkDebugMessages(this->wasm_module,
                                       &this->warduino->program_state);
}
