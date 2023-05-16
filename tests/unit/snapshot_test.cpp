#include <iostream>

#include "../../src/WARDuino.h"
#include "example_code/fac/fac_wasm.h"
#include "gtest/gtest.h"
#include "shared/interruptfixture.h"

class Snapshot : public InterruptFixture {
   protected:
    Snapshot() : InterruptFixture("Snapshot", fac_wasm, fac_wasm_len) {}

    void SetUp() override {
        InterruptFixture::SetUp();
        Block* func = this->moduleCompanion->getMainFunction();
        this->callstackBuilder->pushFunctionCall(func->fidx);
    }

    void failSnapshotNotReceived() {
        this->failAndPrintAllReceivedMessages(
            "Snapshot did not print expected JSON. Received lines:\n");
    }
};

TEST_F(Snapshot, IsValidJSON) {
    this->debugger->snapshot((this->wasm_module));
    nlohmann::basic_json<> fullDump{};
    if (!this->dbgOutput->getJSONReply(&fullDump)) {
        this->failSnapshotNotReceived();
    }
}

TEST_F(Snapshot, PCIsVirtualAddress) {
    // in setup() main already pushed on top of the callstack
    // and pc set to start of main

    Block* main = this->moduleCompanion->getMainFunction();
    uint32_t expectedPC = main->start_ptr - this->wasm_module->bytes;
    this->debugger->snapshot(this->wasm_module);

    nlohmann::basic_json<> parsed{};
    if (!this->dbgOutput->getJSONReply(&parsed)) {
        this->failSnapshotNotReceived();
    }
    try {
        uint32_t pc = parsed["pc"];
        ASSERT_EQ(pc, expectedPC)
            << "snapshot does not print the expected pc virtual address";
        return;
    } catch (const nlohmann::detail::type_error& e) {
        FAIL() << "printed json does not have expected pc key or pc "
                  "cannot be converted to uint32_t\n"
               << "exception message: " << e.what();
    }
}

TEST_F(Snapshot, BreakpointsAreVirtualAddresses) {
    std::vector<uint32_t> virtualBPs = {0, 1, 2, 3};
    for (int i = 0; i < virtualBPs.size(); i++) {
        auto bp = this->wasm_module->bytes + virtualBPs[i];
        this->debugger->addBreakpoint(bp);
    }

    this->debugger->snapshot(this->wasm_module);

    nlohmann::basic_json<> parsed{};
    if (!this->dbgOutput->getJSONReply(&parsed)) {
        this->failSnapshotNotReceived();
    }
    try {
        std::set<uint32_t> bps = parsed["breakpoints"];
        ASSERT_EQ(bps.size(), virtualBPs.size())
            << "expected #" << virtualBPs.size() << " breakpoints";
        for (int i = 0; i < virtualBPs.size(); i++) {
            auto bpVirtualAddr = virtualBPs[i];
            ASSERT_TRUE(bps.count(bpVirtualAddr))
                << "Breakpoint with virtual address " << bpVirtualAddr
                << "is missing ";
        }
    } catch (const nlohmann::detail::type_error& e) {
        FAIL() << "printed json does not have expected breakpoints key "
                  "or breakpoints "
                  "cannot be converted to a set of uint32_t values\n"
               << "exception message: " << e.what();
    }
}

using SnapshotCallstackFrame = Snapshot;
TEST_F(SnapshotCallstackFrame, FunctionBlockKeyIsZero) {
    // in setup() main already pushed on top of the callstack
    Block* main = this->moduleCompanion->getMainFunction();
    uint32_t facMainFidx = main->fidx;

    this->debugger->snapshot(this->wasm_module);

    nlohmann::basic_json<> parsed{};
    if (!this->dbgOutput->getJSONReply(&parsed)) {
        this->failSnapshotNotReceived();
    }
    try {
        nlohmann::basic_json<> frame = parsed["callstack"][0];
        std::string fidxAsHexa = frame["fidx"];
        uint32_t fidx = std::strtoul(fidxAsHexa.c_str(), nullptr, 16);
        ASSERT_EQ(fidx, facMainFidx)
            << "Frame does not belong to expected function";
        ASSERT_EQ(0, frame["block_key"]) << "Function Block key should be zero";
        return;
    } catch (const nlohmann::detail::type_error& e) {
        FAIL() << "printed callstack json does not have key(s) or "
                  "corresponding values cannot be converted to "
                  "expected type.\n"
               << "exception message: " << e.what();
    }
}

TEST_F(SnapshotCallstackFrame, EventBlockKeyIsZero) {
    // in setup() main already pushed on top of the callstack
    // push on the callstack one event block frame
    uint8_t event_type = 0xfe;
    this->callstackBuilder->pushGuard(event_type);

    this->debugger->snapshot(this->wasm_module);

    nlohmann::basic_json<> parsed{};
    if (!this->dbgOutput->getJSONReply(&parsed)) {
        this->failSnapshotNotReceived();
    }
    try {
        nlohmann::basic_json<> frame = parsed["callstack"][1];
        ASSERT_EQ(frame["type"], event_type) << "Invalid frame type";
        ASSERT_EQ(frame["block_key"], 0) << "Function Block key should be zero";
        return;
    } catch (const nlohmann::detail::type_error& e) {
        FAIL() << "printed callstack json does not have key(s) or "
                  "corresponding values cannot be converted to "
                  "expected type.\n"
               << "exception message: " << e.what();
    }
}

TEST_F(SnapshotCallstackFrame, ProxyBlockKeyIsZero) {
    // in setup() main already pushed on top of the callstack
    // push on the callstack one proxy block frame
    uint8_t proxy_type = 0xff;
    this->callstackBuilder->pushGuard(proxy_type);
    this->debugger->snapshot(this->wasm_module);

    nlohmann::basic_json<> parsed{};
    if (!this->dbgOutput->getJSONReply(&parsed)) {
        this->failSnapshotNotReceived();
    }
    try {
        nlohmann::basic_json<> frame = parsed["callstack"][1];
        ASSERT_EQ(frame["type"], proxy_type) << "Invalid frame type";
        ASSERT_EQ(frame["block_key"], 0) << "Function Block key should be zero";
        return;
    } catch (const nlohmann::detail::type_error& e) {
        FAIL() << "printed callstack json does not have key(s) or "
                  "corresponding values cannot be converted to "
                  "expected type.\n"
               << "exception message: " << e.what();
    }
}

TEST_F(SnapshotCallstackFrame, ReturnAddressIsVirtualAddress) {
    /* In setup() main already pushed on top of the callstack
     * Here we push once again the main on the callstack.
     * Callstack contains 2 frames in the following order:
     * 1. one frame for main function call where the sp, fp, return address
     * point to unexisting code
     * 2. one frame for a main function where the return address points to start
     * of main function of the first callstack frame
     */

    Block* func = this->moduleCompanion->getMainFunction();
    uint32_t facMainFidx = func->fidx;
    this->callstackBuilder->pushFunctionCall(facMainFidx);

    uint32_t expectedReturnAddr =
        this->wasm_module->pc_ptr - this->wasm_module->bytes;

    this->debugger->snapshot(this->wasm_module);

    nlohmann::basic_json<> parsed{};
    if (!this->dbgOutput->getJSONReply(&parsed)) {
        this->failSnapshotNotReceived();
    }
    try {
        nlohmann::basic_json<> frame1 = parsed["callstack"][0];
        ASSERT_EQ(0, frame1["ra"]) << "invalid first frame's return addr";
        ASSERT_EQ(-1, frame1["sp"]) << "invalid first frame's sp";
        ASSERT_EQ(-1, frame1["fp"]) << "invalid first frame's fp";

        nlohmann::basic_json<> frame2 = parsed["callstack"][1];
        ASSERT_EQ(expectedReturnAddr, frame2["ra"])
            << "invalid first frame's return addr";
        // sp and fp should increment to account for local var of main func
        ASSERT_EQ(0, frame2["sp"]) << "invalid first frame's sp";
        ASSERT_EQ(0, frame2["fp"]) << "invalid first frame's fp";

        return;
    } catch (const nlohmann::detail::type_error& e) {
        FAIL() << "printed callstack json does not have key(s) or "
                  "corresponding values cannot be converted to "
                  "expected type.\n"
               << "exception message: " << e.what();
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}