#include "../../src/Utils/util.h"
#include "example_code/fac/fac_wasm.h"
#include "gtest/gtest.h"
#include "shared/interruptfixture.h"
#include "shared/json_companion.h"

class Inspect : public InterruptFixture {
   private:
    void encodeB16(uint16_t value, uint8_t* buff) {
        buff[0] = (value >> 8) & 0xFF;
        buff[1] = value & 0xFF;
    }

    void prepareInterrupt(std::string* dest) {
        // interruptSize
        // 1: store interrupt
        // + 2: store size of interrupt
        // + size: one byte per kind of state inspected
        uint32_t interruptSize = 1 + 2 + this->stateToInspect.size();

        uint8_t* interrupt = (uint8_t*)malloc(interruptSize);
        uint32_t offset = 0;
        interrupt[offset++] = this->interruptNr;
        encodeB16(this->stateToInspect.size(), interrupt + offset);
        offset += 2;

        for (uint8_t state : stateToInspect) {
            interrupt[offset++] = state;
        }

        char* hexa = (char*)malloc(interruptSize * 2 + 2);  // +1 for newline

        chars_as_hexa((unsigned char*)hexa, interrupt, interruptSize);
        hexa[interruptSize * 2] = '\n';
	hexa[interruptSize * 2 + 1] = '\0';

        free(interrupt);
        dest->assign(hexa);
    }

   protected:
    Inspect()
        : InterruptFixture("Inspect", interruptInspect, fac_wasm,
                           fac_wasm_len) {}

    std::vector<uint8_t> stateToInspect{};

    void SetUp() override {
        InterruptFixture::SetUp();
        Block* func = this->moduleCompanion->getMainFunction();
        this->callstackBuilder->pushFunctionCall(func->fidx);
    }

    void TearDown() override {
        InterruptFixture::TearDown();
        this->stateToInspect.clear();
    }

    void failInspectNotReceived() {
        this->failAndPrintAllReceivedMessages(
            "Snapshot did not print expected JSON. Received lines:\n");
    }

    void inspect() {
        std::string msg{};
        this->prepareInterrupt(&msg);
        this->debugger->addDebugMessage(msg.size(), (uint8_t*)msg.c_str());
        this->debugger->checkDebugMessages(this->wasm_module,
                                           &this->warduino->program_state);
    }
};

TEST_F(Inspect, InspectNothingShouldGiveEmptyJSON) {
    this->inspect();

    nlohmann::basic_json<> parsed{};
    if (!this->dbgOutput->getJSONReply(&parsed)) {
        this->failInspectNotReceived();
        return;
    }

    JSONCompanion comp{parsed};
    ASSERT_TRUE(comp.containsNoKeys())
        << "Inspect of nothing should print an empty json";
}

TEST_F(Inspect, InspectPC) {
    this->stateToInspect.push_back(pcState);
    this->inspect();

    nlohmann::basic_json<> parsed{};
    if (!this->dbgOutput->getJSONReply(&parsed)) {
        this->failInspectNotReceived();
        return;
    }

    JSONCompanion comp{parsed};
    ASSERT_TRUE(comp.containsKey({"pc"}))
        << fullErrorMessage("Inspect did not print the expected PC state");
    ASSERT_TRUE(comp.containsOnlyKeys({"pc"}))
        << fullErrorMessage("Inspect did print more than just the PC state");
}

TEST_F(Inspect, InspectBreakpoints) {
    this->stateToInspect.push_back(breakpointsState);
    this->inspect();

    nlohmann::basic_json<> parsed{};
    if (!this->dbgOutput->getJSONReply(&parsed)) {
        this->failInspectNotReceived();
        return;
    }

    JSONCompanion comp{parsed};
    ASSERT_TRUE(comp.containsKey({"breakpoints"})) << fullErrorMessage(
        "Inspect did not print the expected breakpoints state");
    ASSERT_TRUE(comp.containsOnlyKeys({"breakpoints"})) << fullErrorMessage(
        "Inspect did print more than just the breakpoints state");
}

TEST_F(Inspect, InspectCallstack) {
    this->stateToInspect.push_back(callstackState);
    this->inspect();

    nlohmann::basic_json<> parsed{};
    if (!this->dbgOutput->getJSONReply(&parsed)) {
        this->failInspectNotReceived();
        return;
    }

    JSONCompanion comp{parsed};
    ASSERT_TRUE(comp.containsKey({"callstack"})) << fullErrorMessage(
        "Inspect did not print the expected callstack state");
    ASSERT_TRUE(comp.containsOnlyKeys({"callstack"})) << fullErrorMessage(
        "Inspect did print more than just the callstack state");
}

TEST_F(Inspect, InspectGlobals) {
    this->stateToInspect.push_back(globalsState);
    this->inspect();

    nlohmann::basic_json<> parsed{};
    if (!this->dbgOutput->getJSONReply(&parsed)) {
        this->failInspectNotReceived();
        return;
    }

    JSONCompanion comp{parsed};
    ASSERT_TRUE(comp.containsKey({"globals"}))
        << fullErrorMessage("Inspect did not print the expected globals state");
    ASSERT_TRUE(comp.containsOnlyKeys({"globals"})) << fullErrorMessage(
        "Inspect did print more than just the globals state");
}

TEST_F(Inspect, InspectTable) {
    this->stateToInspect.push_back(tableState);
    this->inspect();

    nlohmann::basic_json<> parsed{};
    if (!this->dbgOutput->getJSONReply(&parsed)) {
        this->failInspectNotReceived();
        return;
    }

    JSONCompanion comp{parsed};
    ASSERT_TRUE(comp.containsKey({"table"}))
        << fullErrorMessage("Inspect did not print the expected table state");
    ASSERT_TRUE(comp.containsOnlyKeys({"table"}))
        << fullErrorMessage("Inspect did print more than just the table state");
}

TEST_F(Inspect, InspectMemory) {
    this->stateToInspect.push_back(memoryState);
    this->inspect();

    nlohmann::basic_json<> parsed{};
    if (!this->dbgOutput->getJSONReply(&parsed)) {
        this->failInspectNotReceived();
        return;
    }

    JSONCompanion comp{parsed};
    ASSERT_TRUE(comp.containsKey({"memory"}))
        << fullErrorMessage("Inspect did not print the expected memory state");
    ASSERT_TRUE(comp.containsOnlyKeys({"memory"})) << fullErrorMessage(
        "Inspect did print more than just the memory state");
}

TEST_F(Inspect, InspectBranchingTable) {
    this->stateToInspect.push_back(branchingTableState);
    this->inspect();

    nlohmann::basic_json<> parsed{};
    if (!this->dbgOutput->getJSONReply(&parsed)) {
        this->failInspectNotReceived();
        return;
    }

    JSONCompanion comp{parsed};
    ASSERT_TRUE(comp.containsKey({"br_table"})) << fullErrorMessage(
        "Inspect did not print the expected branching table state");
    ASSERT_TRUE(comp.containsOnlyKeys({"br_table"})) << fullErrorMessage(
        "Inspect did print more than just the brancing table state.");
}

TEST_F(Inspect, InspectStack) {
    this->stateToInspect.push_back(stackState);
    this->inspect();

    nlohmann::basic_json<> parsed{};
    if (!this->dbgOutput->getJSONReply(&parsed)) {
        this->failInspectNotReceived();
        return;
    }

    JSONCompanion comp{parsed};
    ASSERT_TRUE(comp.containsKey({"stack"}))
        << fullErrorMessage("Inspect did not print the expected stack state");
    ASSERT_TRUE(comp.containsOnlyKeys({"stack"}))
        << fullErrorMessage("Inspect did print more than just the stack state");
}

TEST_F(Inspect, InspectCallbacks) {
    this->stateToInspect.push_back(callbacksState);
    this->inspect();

    nlohmann::basic_json<> parsed{};
    if (!this->dbgOutput->getJSONReply(&parsed)) {
        this->failInspectNotReceived();
        return;
    }

    JSONCompanion comp{parsed};
    ASSERT_TRUE(comp.containsKey({"callbacks"})) << fullErrorMessage(
        "Inspect did not print the expected callbacks state");
    ASSERT_TRUE(comp.containsOnlyKeys({"callbacks"})) << fullErrorMessage(
        "Inspect did print more than just the callbacks state");
}

TEST_F(Inspect, InspectEvents) {
    this->stateToInspect.push_back(eventsState);
    this->inspect();

    nlohmann::basic_json<> parsed{};
    if (!this->dbgOutput->getJSONReply(&parsed)) {
        this->failInspectNotReceived();
        return;
    }

    JSONCompanion comp{parsed};
    ASSERT_TRUE(comp.containsKey({"events"}))
        << fullErrorMessage("Inspect did not print the expected events state");
    ASSERT_TRUE(comp.containsOnlyKeys({"events"})) << fullErrorMessage(
        "Inspect did print more than just the events state");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
