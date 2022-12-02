#include <stdio.h>
#include <unistd.h>

#include "../../src/WARDuino.h"
#include "example_code/fac/fac_wasm.h"
#include "example_code/blink/blink_wasm.h"
#include "gtest/gtest.h"

class StateModule : public ::testing::Test {
   protected:
    WARDuino* warduino;
    Module* wasm_module;

    StateModule() : warduino(WARDuino::instance()) {}

    ~StateModule() override {}

    void SetUp() override {
        wasm_module = new Module;
        Options opts = {.disable_memory_bounds = false,
                        .mangle_table_index = false,
                        .dlsym_trim_underscore = false,
                        .return_exception = true};
        wasm_module->warduino = this->warduino;
    }

    void TearDown() override { delete wasm_module; }
};

TEST_F(StateModule, DISABLED_InitiallyEmpty) {
    ASSERT_NE(wasm_module, nullptr);
    EXPECT_EQ(wasm_module->types, nullptr);
    EXPECT_EQ(wasm_module->functions, nullptr);
    EXPECT_EQ(wasm_module->globals, nullptr);
    EXPECT_EQ(wasm_module->table.entries, nullptr);
    EXPECT_EQ(wasm_module->memory.bytes, nullptr);
    EXPECT_EQ(wasm_module->stack, nullptr);
    EXPECT_EQ(wasm_module->callstack, nullptr);
    EXPECT_EQ(wasm_module->br_table, nullptr);
}

TEST_F(StateModule, DISABLED_FacLoadsWithoutTableGlobalsAndMemory) {
    warduino->load_module_state(wasm_module, fac_wasm, fac_wasm_len);
    ASSERT_NE(wasm_module, nullptr);
    EXPECT_NE(wasm_module->types, nullptr);
    EXPECT_NE(wasm_module->functions, nullptr);
    EXPECT_NE(wasm_module->stack, nullptr);
    EXPECT_NE(wasm_module->callstack, nullptr);
    EXPECT_NE(wasm_module->br_table, nullptr);

    // memory, table, and globals are not used in fac.wast
    EXPECT_EQ(wasm_module->memory.bytes, nullptr);
    EXPECT_EQ(wasm_module->globals, nullptr);
    EXPECT_EQ(wasm_module->table.entries, nullptr);
}

TEST_F(StateModule, DISABLED_FacStateFrees) {
    warduino->load_module_state(wasm_module, fac_wasm, fac_wasm_len);
    ASSERT_NE(wasm_module->types, nullptr);
    ASSERT_NE(wasm_module->functions, nullptr);
    ASSERT_NE(wasm_module->stack, nullptr);
    ASSERT_NE(wasm_module->callstack, nullptr);
    ASSERT_NE(wasm_module->br_table, nullptr);

    // memory, table, and globals are not used in fac.wast
    ASSERT_EQ(wasm_module->memory.bytes, nullptr);
    ASSERT_EQ(wasm_module->globals, nullptr);
    ASSERT_EQ(wasm_module->table.entries, nullptr);

    warduino->free_module_state(wasm_module);
    EXPECT_EQ(wasm_module->types, nullptr);
    EXPECT_EQ(wasm_module->functions, nullptr);
    EXPECT_EQ(wasm_module->globals, nullptr);
    EXPECT_EQ(wasm_module->table.entries, nullptr);
    EXPECT_EQ(wasm_module->memory.bytes, nullptr);
    EXPECT_EQ(wasm_module->stack, nullptr);
    EXPECT_EQ(wasm_module->callstack, nullptr);
    EXPECT_EQ(wasm_module->br_table, nullptr);
}

TEST_F(StateModule, DISABLED_FacFreeingStatePreservesOptions) {
    Options opts = wasm_module->options;
    warduino->load_module_state(wasm_module, fac_wasm, fac_wasm_len);
    warduino->free_module_state(wasm_module);
    Options opts2 = wasm_module->options;
    EXPECT_EQ(opts.disable_memory_bounds, opts2.disable_memory_bounds);
    EXPECT_EQ(opts.dlsym_trim_underscore, opts2.dlsym_trim_underscore);
    EXPECT_EQ(opts.mangle_table_index, opts2.mangle_table_index);
    EXPECT_EQ(opts.return_exception, opts2.return_exception);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
