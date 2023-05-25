#include <iostream>

#include "../../src/WARDuino.h"
#include "example_code/fac/fac_wasm.h"
#include "gtest/gtest.h"
#include "shared/interruptfixture.h"

class DumpFull : public InterruptFixture {
   private:
    size_t interruptSize = 3;
    uint8_t interrupt[3] = {'1', '2', '\n'};

   protected:
    DumpFull()
        : InterruptFixture("dumpfull", interruptDUMPFull, fac_wasm,
                           fac_wasm_len) {}

    void SetUp() override {
        InterruptFixture::SetUp();
        Block* mainFunc = this->moduleCompanion->getMainFunction();
        this->callstackBuilder->pushFunctionCall(mainFunc->fidx);
    }

    void failDumpFullNotReceived() {
        this->failAndPrintAllReceivedMessages(
            "Dumpfull did not print expected JSON. Received lines:\n");
    }

    void doFullDump() {
        this->debugger->addDebugMessage(interruptSize, interrupt);
        this->debugger->checkDebugMessages(this->wasm_module,
                                           &this->warduino->program_state);
    }
};

TEST_F(DumpFull, IsValidJSON) {
    this->doFullDump();
    nlohmann::basic_json<> fullDump{};
    if (!this->dbgOutput->getJSONReply(&fullDump)) {
        this->failDumpFullNotReceived();
    }
}

TEST_F(DumpFull, IsPCVirtualAddress) {
    // in setup() main already pushed on top of the callstack
    // and pc set to start of main

    Block* main = this->moduleCompanion->getMainFunction();
    uint32_t expectedPC = main->start_ptr - this->wasm_module->bytes;

    this->doFullDump();

    nlohmann::basic_json<> parsed{};
    if (!this->dbgOutput->getJSONReply(&parsed)) {
        this->failDumpFullNotReceived();
    }
    try {
        uint32_t pc = parsed["pc"];
        ASSERT_EQ(pc, expectedPC)
            << "dumpfull does not print the expected pc virtual address";
        return;
    } catch (const nlohmann::detail::type_error& e) {
        FAIL() << "printed json does not have expected pc key or pc "
                  "cannot be converted to uint32_t\n"
               << "exception message: " << e.what();
    }
}

TEST_F(DumpFull, BreakpointsAreVirtualAddresses) {
    std::vector<uint32_t> virtualBPs = {0, 1, 2, 3};
    for (int i = 0; i < virtualBPs.size(); i++) {
        auto bp = this->wasm_module->bytes + virtualBPs[i];
        this->warduino->debugger->addBreakpoint(bp);
    }

    this->doFullDump();

    nlohmann::basic_json<> parsed{};
    if (!this->dbgOutput->getJSONReply(&parsed)) {
        this->failDumpFullNotReceived();
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

TEST_F(DumpFull, FunctionsFromAndToAreVirtualAddresses) {
    this->doFullDump();

    nlohmann::basic_json<> parsed{};
    if (!this->dbgOutput->getJSONReply(&parsed)) {
        this->failDumpFullNotReceived();
        return;
    }
    try {
        auto funcs = parsed["functions"];
        uint32_t nonPrimitiveFuncs =
            this->wasm_module->function_count - this->wasm_module->import_count;
        ASSERT_EQ(funcs.size(), nonPrimitiveFuncs)
            << "dumpfull did not print all functions";
        uint32_t funcIdx = this->wasm_module->import_count;
        for (int i = 0; i < funcs.size(); i++) {
            auto parsedFunc = funcs[i];
            uint32_t from = parsedFunc["from"];
            uint32_t to = parsedFunc["to"];

            Block fun = this->wasm_module->functions[funcIdx++];
            uint32_t expectedFrom = fun.start_ptr - this->wasm_module->bytes;
            uint32_t expectedTo = fun.end_ptr - this->wasm_module->bytes;

            ASSERT_EQ(from, expectedFrom)
                << "from address of func does not match expected address";
            ASSERT_EQ(to, expectedTo)
                << "to address of func does not match expected address";
        }
        return;
    } catch (const nlohmann::detail::type_error& e) {
        FAIL() << "printed json does not have expected pc key or pc "
                  "cannot be converted to uint32_t\n"
               << "exception message: " << e.what();
    }
}

using DumpFullCallstackFrame = DumpFull;

TEST_F(DumpFullCallstackFrame, FirstFramePreservedReturnAddressIsZero) {
    // in setup() main already pushed on top of the callstack
    this->doFullDump();

    nlohmann::basic_json<> parsed{};
    if (!this->dbgOutput->getJSONReply(&parsed)) {
        this->failDumpFullNotReceived();
        return;
    }
    try {
        nlohmann::basic_json<> frame = parsed["callstack"][0];
        ASSERT_EQ(-1, frame["ra"]) << "First Frame should have as return "
                                      "address -1 since it points to nothing";
        return;
    } catch (const nlohmann::detail::type_error& e) {
        FAIL() << "printed callstack json does not have key(s) or "
                  "corresponding values cannot be converted to "
                  "expected type.\n"
               << "exception message: " << e.what();
    }
}

TEST_F(DumpFullCallstackFrame, FirstFramePreservedCallsiteAddressIsZero) {
    // in setup() main already pushed on top of the callstack
    this->doFullDump();

    nlohmann::basic_json<> parsed{};
    if (!this->dbgOutput->getJSONReply(&parsed)) {
        this->failDumpFullNotReceived();
        return;
    }
    try {
        nlohmann::basic_json<> frame = parsed["callstack"][0];
        ASSERT_EQ(-1, frame["callsite"])
            << "First Frame callsite address should be "
               "-1 as no other function called it";
        return;
    } catch (const nlohmann::detail::type_error& e) {
        FAIL() << "printed callstack json does not have key(s) or "
                  "corresponding values cannot be converted to "
                  "expected type.\n"
               << "exception message: " << e.what();
    }
}

TEST_F(DumpFullCallstackFrame, FunctionFrameStartAddressIsVirtual) {
    // in setup() main already pushed on top of the callstack
    Block* mainFunc = this->moduleCompanion->getMainFunction();
    uint32_t startAddr = mainFunc->start_ptr - this->wasm_module->bytes;
    uint32_t endAddr = mainFunc->end_ptr - this->wasm_module->bytes;

    this->doFullDump();

    nlohmann::basic_json<> parsed{};
    if (!this->dbgOutput->getJSONReply(&parsed)) {
        this->failDumpFullNotReceived();
        return;
    }
    try {
        nlohmann::basic_json<> mainFunFrame = parsed["callstack"][0];
        ASSERT_EQ(startAddr, mainFunFrame["start"])
            << "Function Frame start address does not match expected start "
               "address";
        return;
    } catch (const nlohmann::detail::type_error& e) {
        FAIL() << "printed callstack json does not have key(s) or "
                  "corresponding values cannot be converted to "
                  "expected type.\n"
               << "exception message: " << e.what();
    }
}

TEST_F(DumpFullCallstackFrame, FunctionFrameReturnAddressIsVirtual) {
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

    this->doFullDump();

    nlohmann::basic_json<> parsed{};
    if (!this->dbgOutput->getJSONReply(&parsed)) {
        this->failDumpFullNotReceived();
        return;
    }
    try {
        nlohmann::basic_json<> frame1 = parsed["callstack"][0];
        ASSERT_EQ(-1, frame1["ra"]) << "invalid first frame's return addr";
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

TEST_F(DumpFullCallstackFrame, FunctionFrameCallsiteAdressIsVirtual) {
    /* In setup() main already pushed on top of the callstack
     * Here we push once again the main on the callstack.
     * Callstack contains 2 frames in the following order:
     * 1. one frame for main function call where the sp, fp, return address
     * point to unexisting code
     * 2. one frame for a main function where the return address points to start
     * of main function of the first callstack frame
     *
     * The callsite for the first frame should be unexisting i.e., 0.
     * The callsite for the second frame should be the return address minus 2
     */

    Block* func = this->moduleCompanion->getMainFunction();
    uint32_t facMainFidx = func->fidx;
    this->callstackBuilder->pushFunctionCall(facMainFidx);
    int frame1ExpectedCallsite = -1;
    uint32_t frame2ExpectedCallsite =
        (this->wasm_module->pc_ptr - 2) - this->wasm_module->bytes;

    this->doFullDump();

    nlohmann::basic_json<> parsed{};
    if (!this->dbgOutput->getJSONReply(&parsed)) {
        this->failDumpFullNotReceived();
        return;
    }
    try {
        nlohmann::basic_json<> frame1 = parsed["callstack"][0];
        ASSERT_EQ(frame1ExpectedCallsite, frame1["callsite"])
            << "invalid first frame's callsite address";

        nlohmann::basic_json<> frame2 = parsed["callstack"][1];
        ASSERT_EQ(frame2ExpectedCallsite, frame2["callsite"])
            << "invalid second frame's callsite address";
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
