#include <iostream>

#include "../../src/Utils/util.h"
#include "example_code/fac/fac_wasm.h"
#include "gtest/gtest.h"
#include "shared/interruptfixture.h"
#include "shared/json_companion.h"
#include "shared/snapshot_state_encoder.h"

/*
 * Currently it is possible to load any snapshot without any restrictions.
 * For instance, we could load a snapshot that loads more global variables than
 * the associated module has. This is certainly incorrect and should be
 * prohibited. Nevertheless, we will temporarily allow it. A future PR will
 * refactor the way how state is loaded and fix those inconsistencies.
 */

class LoadSnapshot : public InterruptFixture {
   protected:
    SnapshotBinaryEncoder* state = {};
    LoadSnapshot()
        : InterruptFixture("LoadSnapshot", interruptLoadSnapshot, fac_wasm,
                           fac_wasm_len) {}

    void SetUp() override {
        InterruptFixture::SetUp();
        this->state =
            new SnapshotBinaryEncoder(this->interruptNr, this->wasm_module);
    }

    void TearDown() override {
        InterruptFixture::TearDown();
        delete this->state;
    }

    void sendMessage(std::string msg) {
        auto size = msg.size();
        const uint8_t* content = (const uint8_t*)msg.c_str();
        this->debugger->addDebugMessage(size, content);
        this->debugger->checkDebugMessages(this->wasm_module,
                                           &this->warduino->program_state);
    }

    void sendFirstMessageWithEmptyState() {
        uint32_t amountGlobals = 0;
        uint32_t table_initial = 0;
        uint32_t table_max = 0;
        uint32_t table_size = 0;
        uint32_t mem_max = 0;
        uint32_t mem_initial = 0;
        uint32_t mem_size = 0;
        std::string firstMessage{};
        this->state->createFirstMessage(&firstMessage, amountGlobals,
                                        table_initial, table_max, table_size,
                                        mem_max, mem_initial, mem_size);
        this->sendMessage(firstMessage);
    }
};

using LoadSnapshotCallstack = LoadSnapshot;

TEST_F(LoadSnapshot, FirstMessage) {
    uint32_t amountGlobals = 3;
    uint32_t table_initial = 1;
    uint32_t table_max = 4;
    uint32_t table_size = 2;
    uint32_t mem_max = 2;
    uint32_t mem_initial = 0;
    uint32_t mem_size = 1;
    std::string firstMessage{};
    state->createFirstMessage(&firstMessage, amountGlobals, table_initial,
                              table_max, table_size, mem_max, mem_initial,
                              mem_size);
    this->sendMessage(firstMessage);

    // send no state
    std::string msg;
    this->state->createStateMessage(&msg);
    this->sendMessage(msg);

    // the first message sets global count to 0
    // subsequent messages updating globals will increase the global_count
    // A future PR will refactor the loadSnapshot and fix
    // these conceptual inconsistencies
    EXPECT_EQ(this->wasm_module->global_count, 0);

    EXPECT_EQ(this->wasm_module->table.initial, table_initial);
    EXPECT_EQ(this->wasm_module->table.maximum, table_max);
    // the first message sets table size to 0
    // subsequent messages updating table elemens increase the table_size
    // A future PR will refactor the loadSnapshot and fix
    // these conceptual inconsistencies
    EXPECT_EQ(this->wasm_module->table.size, 0);

    EXPECT_EQ(this->wasm_module->memory.initial, mem_initial);
    EXPECT_EQ(this->wasm_module->memory.pages, mem_size);
    EXPECT_EQ(this->wasm_module->memory.maximum, mem_max);
}

TEST_F(LoadSnapshot, PC) {
    this->sendFirstMessageWithEmptyState();
    uint32_t expectedPC = 33;  // random value within wasm
    this->state->encodePC(expectedPC);
    std::string msg{};
    bool isLastMessage = true;
    this->state->createStateMessage(&msg, isLastMessage);
    this->sendMessage(msg);
    uint32_t pc =
        toVirtualAddress(this->wasm_module->pc_ptr, this->wasm_module);
    ASSERT_EQ(pc, expectedPC)
        << "the new PC after updating the state does not match expected value";
}

TEST_F(LoadSnapshot, Breakpoints) {
    this->sendFirstMessageWithEmptyState();
    std::set<uint32_t> breakpoints = {1, 2, 3, 4};  // all valid bps

    this->state->encodeBreakpoints(breakpoints);
    std::string msg{};
    this->state->createStateMessage(&msg);
    this->sendMessage(msg);
    ASSERT_EQ(this->debugger->breakpoints.size(), 4)
        << "expected 4 breakpoints to be loaded";
    for (auto bpPointer : this->debugger->breakpoints) {
        uint32_t bp = toVirtualAddress(bpPointer, this->wasm_module);
        ASSERT_TRUE(breakpoints.count(bp) == 1)
            << " bp after updating the state does not match expected "
               "value";
    }
}

TEST_F(LoadSnapshot, InvalidBreakpointsAreNotLoaded) {
    this->sendFirstMessageWithEmptyState();
    std::set<uint32_t> breakpoints{};
    uint32_t validBP = 1;
    breakpoints.insert(validBP);             // valid bp
    breakpoints.insert(this->wasm_len + 1);  // invalid bp
    breakpoints.insert(this->wasm_len + 2);  // invalid bp
    breakpoints.insert(this->wasm_len + 3);  // invalid bp

    this->state->encodeBreakpoints(breakpoints);
    std::string msg{};
    this->state->createStateMessage(&msg);
    this->sendMessage(msg);
    ASSERT_EQ(this->debugger->breakpoints.size(), 1)
        << "only one breakpoint was valid and should have been kept";
    for (auto bpPointer : this->debugger->breakpoints) {
        uint32_t bp = toVirtualAddress(bpPointer, this->wasm_module);
        ASSERT_EQ(bp, validBP)
            << " the only valid breakpoint is not part of the breakpoints";
    }
}

TEST_F(LoadSnapshot, Callbacks) {
    this->sendFirstMessageWithEmptyState();
    std::vector<Callback> callbacks{};

    // register 3 callbacks for topic10
    std::string topic10 = "interrupt_10";
    std::set<uint32_t> topic10Indexes = {11, 12, 13};
    for (auto tidx : topic10Indexes) {
        callbacks.push_back(Callback(this->wasm_module, topic10.c_str(), tidx));
    }

    // register 2 callbacks for topic22
    std::string topic22 = "interrupt_22";
    std::set<uint32_t> topic22Indexes = {21, 22};
    for (auto tidx : topic22Indexes) {
        callbacks.push_back(Callback(this->wasm_module, topic22.c_str(), tidx));
    }

    this->state->encodeCallbacks(callbacks);
    std::string msg{};
    this->state->createStateMessage(&msg);
    this->sendMessage(msg);

    std::string dumpedCallbacks{};
    try {
        dumpedCallbacks = CallbackHandler::dump_callbacksV2();
        nlohmann::basic_json<> parsed = nlohmann::json::parse(dumpedCallbacks);
        // auto cbs = parsed["callbacks"];

        // testing that callbacks for each topic got registered
        std::set<std::string> expectedTopics{topic22, topic10};
        std::set<std::string> dumpedTopics{};
        for (auto dumpedCallback : parsed["callbacks"]) {
            dumpedTopics.insert(dumpedCallback["callbackid"]);
        }
        ASSERT_EQ(dumpedTopics.size(), 2)
            << "we expect callbacks for 2 different topics";

        for (auto dumpedTopic : dumpedTopics) {
            ASSERT_TRUE(expectedTopics.count(dumpedTopic))
                << "callbacks for unexpected topic " << dumpedTopic
                << " got dumped";
        }

        // testing that for each topic the right amount of callbacks got
        // registered
        for (auto dumpedCallback : parsed["callbacks"]) {
            if (dumpedCallback["callbackid"] != topic10 &&
                dumpedCallback["callbackid"] != topic22) {
                FAIL() << "loaded an invalid callback. " << dumpedCallback;
            }

            std::set<uint32_t> loadedIndexes = dumpedCallback["tableIndexes"];
            std::set<uint32_t> expectedIndexes =
                dumpedCallback["callbackid"] == topic22 ? topic22Indexes
                                                        : topic10Indexes;
            std::string topic =
                dumpedCallback["callbackid"] == topic22 ? topic22 : topic10;
            ASSERT_EQ(expectedIndexes.size(), loadedIndexes.size())
                << "mismatch in the number of loaded table index for callbacks "
                   "with topic "
                << topic;
            for (auto tidx : expectedIndexes) {
                ASSERT_TRUE(loadedIndexes.count(tidx))
                    << "table idx " << tidx
                    << " is not loaded as expected for "
                       "topic "
                    << topic;
            }
        }
    } catch (const nlohmann::detail::parse_error& e) {
        FAIL() << "dumped callbacks is invalid json. Received "
               << dumpedCallbacks;
    }
}

TEST_F(LoadSnapshotCallstack, FuncFrame) {
    this->sendFirstMessageWithEmptyState();

    std::vector<Frame> frames = {};
    uint32_t returnAddress = 33;  // random
    uint32_t sp = 31;             // random
    uint32_t fp = 34;             // random
    Frame f{};
    f.ra_ptr = toPhysicalAddress(returnAddress, this->wasm_module);
    f.sp = sp;
    f.fp = fp;
    Block* main = this->moduleCompanion->getMainFunction();
    f.block = main;
    frames.push_back(f);
    this->state->encodeCallstack(&frames);

    std::string msg{};
    this->state->createStateMessage(&msg);
    this->sendMessage(msg);
    ASSERT_EQ(this->wasm_module->csp, 0);

    Frame f2 = this->wasm_module->callstack[0];
    ASSERT_EQ(f2.sp, sp);
    ASSERT_EQ(f2.fp, fp);
    ASSERT_EQ(toVirtualAddress(f2.ra_ptr, this->wasm_module), returnAddress);
    ASSERT_NE(f2.block, nullptr);
    ASSERT_EQ(f2.block->block_type, 0);
    ASSERT_EQ(f2.block->fidx, main->fidx);
}

TEST_F(LoadSnapshotCallstack, ProxyFrame) {
    this->sendFirstMessageWithEmptyState();

    std::vector<Frame> frames = {};
    uint32_t returnAddress = 33;  // random
    uint32_t sp = 31;             // random
    uint32_t fp = 34;             // random
    Frame f{};
    f.ra_ptr = toPhysicalAddress(returnAddress, this->wasm_module);
    f.sp = sp;
    f.fp = fp;
    Block* proxyguard = (Block*)malloc(sizeof(Block));
    proxyguard->block_type = 0xff;
    f.block = proxyguard;
    frames.push_back(f);
    this->state->encodeCallstack(&frames);

    std::string msg{};
    this->state->createStateMessage(&msg);
    this->sendMessage(msg);
    ASSERT_EQ(this->wasm_module->csp, 0);

    Frame f2 = this->wasm_module->callstack[0];
    ASSERT_EQ(f2.sp, sp);
    ASSERT_EQ(f2.fp, fp);
    ASSERT_EQ(toVirtualAddress(f2.ra_ptr, this->wasm_module), returnAddress);
    ASSERT_NE(f2.block, nullptr);
    ASSERT_EQ(f2.block->block_type, 0xff);
}

TEST_F(LoadSnapshotCallstack, EventFrame) {
    this->sendFirstMessageWithEmptyState();

    std::vector<Frame> frames = {};
    uint32_t returnAddress = 33;  // random
    uint32_t sp = 31;             // random
    uint32_t fp = 34;             // random
    Frame f{};
    f.ra_ptr = toPhysicalAddress(returnAddress, this->wasm_module);
    f.sp = sp;
    f.fp = fp;
    Block* proxyguard = (Block*)malloc(sizeof(Block));
    proxyguard->block_type = 0xfe;
    f.block = proxyguard;
    frames.push_back(f);
    this->state->encodeCallstack(&frames);

    std::string msg{};
    this->state->createStateMessage(&msg);
    this->sendMessage(msg);
    ASSERT_EQ(this->wasm_module->csp, 0);

    Frame f2 = this->wasm_module->callstack[0];
    ASSERT_EQ(f2.sp, sp);
    ASSERT_EQ(f2.fp, fp);
    ASSERT_EQ(toVirtualAddress(f2.ra_ptr, this->wasm_module), returnAddress);
    ASSERT_NE(f2.block, nullptr);
    ASSERT_EQ(f2.block->block_type, 0xfe);
}

TEST_F(LoadSnapshotCallstack, IfFrame) {
    this->sendFirstMessageWithEmptyState();

    std::vector<Frame> frames = {};
    uint32_t returnAddress = 33;  // random
    uint32_t sp = 31;             // random
    uint32_t fp = 34;             // random
    Frame f{};
    f.ra_ptr = toPhysicalAddress(returnAddress, this->wasm_module);
    f.sp = sp;
    f.fp = fp;
    uint8_t iftype = 0x04;
    Block* block = this->moduleCompanion->firstBlock(iftype);
    f.block = block;
    frames.push_back(f);
    this->state->encodeCallstack(&frames);

    std::string msg{};
    this->state->createStateMessage(&msg);
    this->sendMessage(msg);
    ASSERT_EQ(this->wasm_module->csp, 0);

    Frame f2 = this->wasm_module->callstack[0];
    ASSERT_EQ(f2.sp, sp);
    ASSERT_EQ(f2.fp, fp);
    ASSERT_EQ(toVirtualAddress(f2.ra_ptr, this->wasm_module), returnAddress);
    ASSERT_NE(f2.block, nullptr);
    ASSERT_EQ(f2.block, block);
    ASSERT_EQ(f2.block->block_type, iftype);
}

TEST_F(LoadSnapshotCallstack, LoopFrame) {
    this->sendFirstMessageWithEmptyState();

    std::vector<Frame> frames = {};
    uint32_t returnAddress = 33;  // random
    uint32_t sp = 31;             // random
    uint32_t fp = 34;             // random
    Frame f{};
    f.ra_ptr = toPhysicalAddress(returnAddress, this->wasm_module);
    f.sp = sp;
    f.fp = fp;
    uint8_t loopType = 0x03;
    Block* block = this->moduleCompanion->firstBlock(loopType);
    f.block = block;
    frames.push_back(f);
    this->state->encodeCallstack(&frames);

    std::string msg{};
    this->state->createStateMessage(&msg);
    this->sendMessage(msg);
    ASSERT_EQ(this->wasm_module->csp, 0);

    Frame f2 = this->wasm_module->callstack[0];
    ASSERT_EQ(f2.sp, sp);
    ASSERT_EQ(f2.fp, fp);
    ASSERT_EQ(toVirtualAddress(f2.ra_ptr, this->wasm_module), returnAddress);
    ASSERT_NE(f2.block, nullptr);
    ASSERT_EQ(f2.block, block);
    ASSERT_EQ(f2.block->block_type, loopType);
}

TEST_F(LoadSnapshotCallstack, FrameWithNegativeSPAndFP) {
    this->sendFirstMessageWithEmptyState();

    std::vector<Frame> frames = {};
    Frame f{};
    f.ra_ptr = toPhysicalAddress(33, this->wasm_module);
    f.sp = -1;
    f.fp = -33;  // random negative
    Block* main = this->moduleCompanion->getMainFunction();
    f.block = main;
    frames.push_back(f);
    this->state->encodeCallstack(&frames);

    std::string msg{};
    this->state->createStateMessage(&msg);
    this->sendMessage(msg);
    Frame f2 = this->wasm_module->callstack[0];
    ASSERT_EQ(f2.sp, -1) << "A frame with a negative sp should be loaded";
    ASSERT_EQ(f2.fp, -33) << "A frame with a negative fp should be loaded";
}

TEST_F(LoadSnapshotCallstack, FrameWithNullPtrAsReturnAddresShouldBeLoaded) {
    // the return address should become a nullptr
    this->sendFirstMessageWithEmptyState();

    std::vector<Frame> frames = {};
    Frame f{};
    f.ra_ptr = nullptr;
    f.sp = -1;
    f.fp = -33;  // random negative
    Block* main = this->moduleCompanion->getMainFunction();
    f.block = main;
    frames.push_back(f);
    this->state->encodeCallstack(&frames);

    std::string msg{};
    this->state->createStateMessage(&msg);
    this->sendMessage(msg);
    Frame f2 = this->wasm_module->callstack[0];
    ASSERT_EQ(f2.ra_ptr, nullptr);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
