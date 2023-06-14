#include "../../src/WARDuino.h"
#include "callstackbuilder.h"
#include "dbgoutput.h"
#include "gtest/gtest.h"
#include "modulecompanion.h"

class InterruptFixture : public ::testing::Test {
   protected:
    uint8_t* wasm{};
    size_t wasm_len{};
    WARDuino* warduino;
    Module* wasm_module;
    DBGOutput* dbgOutput{};
    Debugger* debugger{};
    const char* interruptName{};
    const uint8_t interruptNr{};
    CallstackBuilder* callstackBuilder{};
    ModuleCompanion* moduleCompanion{};

    InterruptFixture(const char* t_interruptName, uint8_t t_interruptNr,
                     uint8_t* t_wasm, size_t t_wasm_len);

    ~InterruptFixture() override;

    void SetUp() override;
    void failAndPrintAllReceivedMessages(const char* failReason);
    void TearDown() override;
    std::string fullErrorMessage(const char* msg);
    void sendInterruptNoPayload(uint8_t interruptNr);
};