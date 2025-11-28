#include "../../src/WARDuino.h"
#include "example_code/blink/blink_wasm.h"
#include "example_code/dimmer/dimmer_wasm.h"
#include "example_code/fac/fac_wasm.h"
#include "gtest/gtest.h"

class FreeingModuleFixture : public ::testing::Test {
   protected:
    WARDuino* warduino;
    Module* wasm_module;
    Options opts;

    FreeingModuleFixture() : warduino(WARDuino::instance()) {}

    ~FreeingModuleFixture() override {}

    void SetUp() override {
        wasm_module = new Module;
        opts = {.disable_memory_bounds = false,
                .mangle_table_index = false,
                .dlsym_trim_underscore = false,
                .return_exception = true};
        wasm_module->options = opts;
        wasm_module->warduino = this->warduino;
    }

    void TearDown() override {
        wasm_module->warduino = nullptr;
        delete wasm_module;
    }
};

TEST_F(FreeingModuleFixture, FreeingModuleStateEmptiesModule) {
    warduino->init_execution_context();
    warduino->instantiate_module(wasm_module, dimmer_wasm, dimmer_wasm_len);
    warduino->free_module_state(wasm_module);
    warduino->free_execution_context();
    EXPECT_EQ(wasm_module->types, nullptr);
    EXPECT_EQ(wasm_module->functions, nullptr);
    EXPECT_EQ(wasm_module->globals, nullptr);
    EXPECT_EQ(wasm_module->table.entries, nullptr);
    EXPECT_EQ(wasm_module->memory.bytes, nullptr);
    EXPECT_EQ(wasm_module->warduino->execution_context, nullptr);
}

TEST_F(FreeingModuleFixture, FreeingStatePreservesOptions) {
    warduino->init_execution_context();
    warduino->instantiate_module(wasm_module, blink_wasm, blink_wasm_len);
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
