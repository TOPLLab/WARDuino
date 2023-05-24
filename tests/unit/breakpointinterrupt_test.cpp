#include <iostream>

#include "../../src/Utils/util.h"
#include "example_code/fac/fac_wasm.h"
#include "gtest/gtest.h"
#include "shared/interruptfixture.h"
#include "shared/serialisation.h"

class BreakpointInterrupt : public InterruptFixture {
   private:
    void sendBpInterrupt(bool add, uint32_t bpAddress) {
        std::string interrupt{{'0', add ? '6' : '7'}};
        interrupt += Serialiser::uint32ToHexString(bpAddress);
        interrupt += '\n';
        this->debugger->addDebugMessage(interrupt.size(),
                                        (uint8_t*)interrupt.c_str());
        this->debugger->checkDebugMessages(this->wasm_module,
                                           &this->warduino->program_state);
    }

   protected:
    BreakpointInterrupt()
        : InterruptFixture("add or remove breakpoint", fac_wasm, fac_wasm_len) {
    }

    void SetUp() override {
        InterruptFixture::SetUp();
        Block* mainFunc = this->moduleCompanion->getMainFunction();
        this->callstackBuilder->pushFunctionCall(mainFunc->fidx);
    }

    void failInterruptAckNotReceived() {
        this->failAndPrintAllReceivedMessages(
            "Debugger's ack for receiving interrupt was not send. "
            "DBGOutput:\n ");
    }

    void failRemoveBreakpointAckNotReceived() {
        this->failAndPrintAllReceivedMessages(
            "Debugger's ack for removing breakpoint was not send."
            "DBGOutput:\n ");
    }

    void failAddBreakpointAckNotReceived() {
        this->failAndPrintAllReceivedMessages(
            "Debugger's ack for adding breakpoint was not send."
            "DBGOutput:\n ");
    }

    void addBreakpoint(uint32_t bpaddress) {
        bool add = true;
        this->sendBpInterrupt(add, bpaddress);
    }

    void removeBreakpoint(uint32_t bpaddress) {
        bool add = false;
        this->sendBpInterrupt(add, bpaddress);
    }
};

TEST_F(BreakpointInterrupt,
       AddingBreakpointExpectsVirtualAddressAndPrintsVirtualAddressAck) {
    auto bpsSizePriorInterrupt = this->debugger->breakpoints.size();
    uint32_t bp = 2;
    this->addBreakpoint(bp);

    // first message is about confirmation interrupt was received
    std::string* confirmInterruptReceived = this->dbgOutput->getLine();
    if (*confirmInterruptReceived == "") {
        this->failInterruptAckNotReceived();
        return;
    }

    std::string expectedMessage = "Interrupt: 6";
    ASSERT_EQ(*confirmInterruptReceived, expectedMessage)
        << "invalid ack for confirming reception of interrupt";

    // second message is about confirmation that bp got added
    std::string* confirmBpAdded = this->dbgOutput->getLine();
    if (*confirmBpAdded == "") {
        this->failAddBreakpointAckNotReceived();
        return;
    }
    expectedMessage = "BP ";
    expectedMessage += std::to_string(bp);
    expectedMessage += "!";
    ASSERT_EQ(*confirmBpAdded, expectedMessage)
        << "invalid ack for confirming adding the breakpoint";
    ASSERT_EQ(bpsSizePriorInterrupt + 1, this->debugger->breakpoints.size())
        << "breakpoint should be added in the breakpoints set";
}

TEST_F(BreakpointInterrupt, InvalidBreakpointIsNotAdded) {
    auto bpsSizePriorInterrupt = this->debugger->breakpoints.size();
    uint32_t bp = this->wasm_module->byte_count + 2;
    this->addBreakpoint(bp);

    // first message is about confirmation interrupt was received
    std::string* confirmInterruptReceived = this->dbgOutput->getLine();
    if (*confirmInterruptReceived == "") {
        this->failInterruptAckNotReceived();
        return;
    }
    std::string expectedMessage = "Interrupt: 6";
    ASSERT_EQ(*confirmInterruptReceived, expectedMessage)
        << "invalid ack for confirming reception of interrupt";

    // second message is about confirming that bp got supposedly added
    std::string* confirmBpAdded = this->dbgOutput->getLine();
    if (*confirmBpAdded == "") {
        this->failAddBreakpointAckNotReceived();
        return;
    }
    expectedMessage = "BP ";
    expectedMessage += std::to_string(bp);
    expectedMessage += "!";
    ASSERT_EQ(*confirmBpAdded, expectedMessage)
        << "invalid ack for confirming adding the breakpoint";
    ASSERT_EQ(bpsSizePriorInterrupt, this->debugger->breakpoints.size())
        << "invalid breakpoint should not be added";
}

TEST_F(BreakpointInterrupt,
       RemovingBreakpointExpectsVirtualAddressAndPrintsVirtualAddressAck) {
    auto bpsSizePriorInterrupt = this->debugger->breakpoints.size();
    uint32_t bp = 2;
    this->removeBreakpoint(bp);

    // first message is about confirmation interrupt was received
    std::string* confirmInterruptReceived = this->dbgOutput->getLine();
    if (*confirmInterruptReceived == "") {
        this->failInterruptAckNotReceived();
        return;
    }

    std::string expectedMessage = "Interrupt: 7";
    ASSERT_EQ(*confirmInterruptReceived, expectedMessage)
        << "invalid ack for confirming reception of interrupt";

    // second message is about confirmation that bp got removed
    std::string* confirmBpRemoved = this->dbgOutput->getLine();
    if (*confirmBpRemoved == "") {
        this->failRemoveBreakpointAckNotReceived();
        return;
    }
    expectedMessage = "BP ";
    expectedMessage += std::to_string(bp);
    expectedMessage += "!";
    ASSERT_EQ(*confirmBpRemoved, expectedMessage)
        << "invalid ack for confirming removing the breakpoint";
    ASSERT_EQ(bpsSizePriorInterrupt, this->debugger->breakpoints.size())
        << "breakpoints should remain same size since bp was not previously "
           "not added";
}

TEST_F(BreakpointInterrupt, InvalidBreakpointIsNotRemoved) {
    auto bpsSizePriorInterrupt = this->debugger->breakpoints.size();
    uint32_t bp = 2;
    this->removeBreakpoint(bp);

    // first message is about confirmation interrupt was received
    std::string* confirmInterruptReceived = this->dbgOutput->getLine();
    if (*confirmInterruptReceived == "") {
        this->failInterruptAckNotReceived();
        return;
    }

    std::string expectedMessage = "Interrupt: 7";
    ASSERT_EQ(*confirmInterruptReceived, expectedMessage)
        << "invalid ack for confirming reception of interrupt";

    // second message is about confirmation that bp got removed
    std::string* confirmBpRemoved = this->dbgOutput->getLine();
    if (*confirmBpRemoved == "") {
        this->failRemoveBreakpointAckNotReceived();
        return;
    }
    expectedMessage = "BP ";
    expectedMessage += std::to_string(bp);
    expectedMessage += "!";
    ASSERT_EQ(*confirmBpRemoved, expectedMessage)
        << "invalid ack for confirming removing the breakpoint";
    ASSERT_EQ(bpsSizePriorInterrupt, this->debugger->breakpoints.size())
        << "breakpoints should remain same size as invalid bp is ignored";
}

TEST_F(BreakpointInterrupt, NotifyBreakpointReachedPrintsVirtualAddress) {
    // notifiy bp reached at address `bp`
    uint32_t bp = 33;
    uint8_t* physicalAddr = toPhysicalAddress(bp, this->wasm_module);

    this->debugger->notifyBreakpoint(this->wasm_module, physicalAddr);

    std::string* notification = this->dbgOutput->getLine();
    ASSERT_NE(*notification, "")
        << "Debugger is expected to print a notification";
    std::string expectedNotification{};
    expectedNotification = "AT ";
    expectedNotification += std::to_string(bp);
    expectedNotification += "!";
    ASSERT_EQ(*notification, expectedNotification)
        << "Invalid at breakpoint notification";
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
