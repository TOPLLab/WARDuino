#include "../../src/Interrupts/interrupt_inspect.h"
#include "../../src/Interrupts/interrupts.h"
#include "../../src/Utils/util.h"
#include "example_code/fac/fac_wasm.h"
#include "gtest/gtest.h"
#include "shared/interruptfixture.h"
#include "shared/json_companion.h"
#include "shared/serialisation.h"

class Inspect : public InterruptFixture {
   private:
    void prepareInterrupt(std::string& dest, uint8_t ackId) {
        std::string interruptStr{};
        Serialiser::uint8ToHexString(this->interruptNr, interruptStr);

        std::string ackStr{};
        Serialiser::uint8ToHexString(ackId, ackStr);

        std::string sizeStr{};
        uint8_t sizeBuffer[2]{};
        Serialiser::encodeB16(this->stateToInspect.size(), sizeBuffer);
        Serialiser::uint8BufferToHex(sizeBuffer, 2, sizeStr);

        dest = interruptStr + ackStr + sizeStr;
        for (uint8_t state : stateToInspect) {
            std::string stateStr{};
            Serialiser::uint8ToHexString(state, stateStr);
            dest += stateStr;
        }
        dest += "\n";
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

    void inspect(uint8_t idAck) {
        std::string msg{};
        this->prepareInterrupt(msg, idAck);
        this->debugger->addDebugMessage(msg.size(), (uint8_t*)msg.c_str());
        this->debugger->checkDebugMessages(this->wasm_module,
                                           &this->warduino->program_state);
    }
};

TEST_F(Inspect, InspectPC) {
    this->stateToInspect.push_back(pcState);

    uint8_t idAck = 33;
    this->inspect(idAck);

    nlohmann::basic_json<> reply{};
    if (!this->dbgOutput->getJSONReply(&reply)) {
        this->failInspectNotReceived();
        return;
    }

    JSONCompanion compReply{reply};
    ASSERT_TRUE(compReply.containsKey({"sub"}))
        << fullErrorMessage("Inspect did not print the sub content");
    auto parsed = reply["sub"];

    JSONCompanion comp{parsed};
    ASSERT_TRUE(comp.containsKey({"pc"}))
        << fullErrorMessage("Inspect did not print the expected PC state");
    ASSERT_TRUE(comp.containsOnlyKeys({"pc"}))
        << fullErrorMessage("Inspect did print more than just the PC state");
}

TEST_F(Inspect, InspectBreakpoints) {
    this->stateToInspect.push_back(breakpointsState);
    uint8_t idAck = 33;
    this->inspect(idAck);

    nlohmann::basic_json<> reply{};
    if (!this->dbgOutput->getJSONReply(&reply)) {
        this->failInspectNotReceived();
        return;
    }

    JSONCompanion compReply{reply};
    ASSERT_TRUE(compReply.containsKey({"sub"}))
        << fullErrorMessage("Inspect did not print the sub content");
    auto parsed = reply["sub"];

    JSONCompanion comp{parsed};
    ASSERT_TRUE(comp.containsKey({"breakpoints"})) << fullErrorMessage(
        "Inspect did not print the expected breakpoints state");
    ASSERT_TRUE(comp.containsOnlyKeys({"breakpoints"})) << fullErrorMessage(
        "Inspect did print more than just the breakpoints state");
}

TEST_F(Inspect, InspectCallstack) {
    this->stateToInspect.push_back(callstackState);

    uint8_t idAck = 1;
    this->inspect(idAck);

    nlohmann::basic_json<> reply{};
    if (!this->dbgOutput->getJSONReply(&reply)) {
        this->failInspectNotReceived();
        return;
    }

    JSONCompanion compReply{reply};
    ASSERT_TRUE(compReply.containsKey({"sub"}))
        << fullErrorMessage("Inspect did not print the sub content");
    auto parsed = reply["sub"];

    JSONCompanion comp{parsed};
    ASSERT_TRUE(comp.containsKey({"callstack"})) << fullErrorMessage(
        "Inspect did not print the expected callstack state");
    ASSERT_TRUE(comp.containsOnlyKeys({"callstack"})) << fullErrorMessage(
        "Inspect did print more than just the callstack state");
}

TEST_F(Inspect, InspectGlobals) {
    this->stateToInspect.push_back(globalsState);
    uint8_t idAck = 1;
    this->inspect(idAck);

    nlohmann::basic_json<> reply{};
    if (!this->dbgOutput->getJSONReply(&reply)) {
        this->failInspectNotReceived();
        return;
    }

    JSONCompanion compReply{reply};
    ASSERT_TRUE(compReply.containsKey({"sub"}))
        << fullErrorMessage("Inspect did not print the sub content");
    auto parsed = reply["sub"];

    JSONCompanion comp{parsed};
    ASSERT_TRUE(comp.containsKey({"globals"}))
        << fullErrorMessage("Inspect did not print the expected globals state");
    ASSERT_TRUE(comp.containsOnlyKeys({"globals"})) << fullErrorMessage(
        "Inspect did print more than just the globals state");
}

TEST_F(Inspect, InspectTable) {
    this->stateToInspect.push_back(tableState);
    uint8_t idAck = 1;
    this->inspect(idAck);

    nlohmann::basic_json<> reply{};
    if (!this->dbgOutput->getJSONReply(&reply)) {
        this->failInspectNotReceived();
        return;
    }

    JSONCompanion compReply{reply};
    ASSERT_TRUE(compReply.containsKey({"sub"}))
        << fullErrorMessage("Inspect did not print the sub content");
    auto parsed = reply["sub"];

    JSONCompanion comp{parsed};
    ASSERT_TRUE(comp.containsKey({"table"}))
        << fullErrorMessage("Inspect did not print the expected table state");
    ASSERT_TRUE(comp.containsOnlyKeys({"table"}))
        << fullErrorMessage("Inspect did print more than just the table state");
}

TEST_F(Inspect, InspectMemory) {
    this->stateToInspect.push_back(memoryState);
    uint8_t idAck = 1;
    this->inspect(idAck);

    nlohmann::basic_json<> reply{};
    if (!this->dbgOutput->getJSONReply(&reply)) {
        this->failInspectNotReceived();
        return;
    }

    JSONCompanion compReply{reply};
    ASSERT_TRUE(compReply.containsKey({"sub"}))
        << fullErrorMessage("Inspect did not print the sub content");
    auto parsed = reply["sub"];

    JSONCompanion comp{parsed};
    ASSERT_TRUE(comp.containsKey({"memory"}))
        << fullErrorMessage("Inspect did not print the expected memory state");
    ASSERT_TRUE(comp.containsOnlyKeys({"memory"})) << fullErrorMessage(
        "Inspect did print more than just the memory state");
}

TEST_F(Inspect, InspectBranchingTable) {
    this->stateToInspect.push_back(branchingTableState);
    uint8_t idAck = 1;
    this->inspect(idAck);

    nlohmann::basic_json<> reply{};
    if (!this->dbgOutput->getJSONReply(&reply)) {
        this->failInspectNotReceived();
        return;
    }

    JSONCompanion compReply{reply};
    ASSERT_TRUE(compReply.containsKey({"sub"}))
        << fullErrorMessage("Inspect did not print the sub content");
    auto parsed = reply["sub"];

    JSONCompanion comp{parsed};
    ASSERT_TRUE(comp.containsKey({"br_table"})) << fullErrorMessage(
        "Inspect did not print the expected branching table state");
    ASSERT_TRUE(comp.containsOnlyKeys({"br_table"})) << fullErrorMessage(
        "Inspect did print more than just the brancing table state.");
}

TEST_F(Inspect, InspectStack) {
    this->stateToInspect.push_back(stackState);
    uint8_t idAck = 1;
    this->inspect(idAck);

    nlohmann::basic_json<> reply{};
    if (!this->dbgOutput->getJSONReply(&reply)) {
        this->failInspectNotReceived();
        return;
    }

    JSONCompanion compReply{reply};
    ASSERT_TRUE(compReply.containsKey({"sub"}))
        << fullErrorMessage("Inspect did not print the sub content");
    auto parsed = reply["sub"];

    JSONCompanion comp{parsed};
    ASSERT_TRUE(comp.containsKey({"stack"}))
        << fullErrorMessage("Inspect did not print the expected stack state");
    ASSERT_TRUE(comp.containsOnlyKeys({"stack"}))
        << fullErrorMessage("Inspect did print more than just the stack state");
}

TEST_F(Inspect, InspectCallbacks) {
    this->stateToInspect.push_back(callbacksState);
    uint8_t idAck = 1;
    this->inspect(idAck);

    nlohmann::basic_json<> reply{};
    if (!this->dbgOutput->getJSONReply(&reply)) {
        this->failInspectNotReceived();
        return;
    }

    JSONCompanion compReply{reply};
    ASSERT_TRUE(compReply.containsKey({"sub"}))
        << fullErrorMessage("Inspect did not print the sub content");
    auto parsed = reply["sub"];

    JSONCompanion comp{parsed};
    ASSERT_TRUE(comp.containsKey({"callbacks"})) << fullErrorMessage(
        "Inspect did not print the expected callbacks state");
    ASSERT_TRUE(comp.containsOnlyKeys({"callbacks"})) << fullErrorMessage(
        "Inspect did print more than just the callbacks state");
}

TEST_F(Inspect, InspectEvents) {
    this->stateToInspect.push_back(eventsState);
    uint8_t idAck = 1;
    this->inspect(idAck);

    nlohmann::basic_json<> reply{};
    if (!this->dbgOutput->getJSONReply(&reply)) {
        this->failInspectNotReceived();
        return;
    }

    JSONCompanion compReply{reply};
    ASSERT_TRUE(compReply.containsKey({"sub"}))
        << fullErrorMessage("Inspect did not print the sub content");
    auto parsed = reply["sub"];

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
