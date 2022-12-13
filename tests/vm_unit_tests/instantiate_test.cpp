#include <future>

#include "../../src/WARDuino.h"
#include "example_code/blink/blink_wasm.h"
#include "example_code/dimmer/dimmer_wasm.h"
#include "example_code/fac/fac_wasm.h"
#include "gtest/gtest.h"

class ModuleFixture : public ::testing::Test {
   protected:
    WARDuino* warduino;
    Module* wasm_module;
    Options opts;

    ModuleFixture() : warduino(WARDuino::instance()) {}

    ~ModuleFixture() override {}

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

TEST_F(ModuleFixture, InitiallyEmpty) {
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

TEST_F(ModuleFixture, FacLoadsWithoutTableGlobalsAndMemory) {
    warduino->instantiate_module(wasm_module, fac_wasm, fac_wasm_len);
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

TEST_F(ModuleFixture, BlinkLoadsWithoutTableAndMemory) {
    warduino->instantiate_module(wasm_module, blink_wasm, blink_wasm_len);
    EXPECT_NE(wasm_module->types, nullptr);
    EXPECT_NE(wasm_module->functions, nullptr);
    EXPECT_NE(wasm_module->stack, nullptr);
    EXPECT_NE(wasm_module->callstack, nullptr);
    EXPECT_NE(wasm_module->br_table, nullptr);
    EXPECT_NE(wasm_module->globals, nullptr);

    // not used in blink.wast
    EXPECT_EQ(wasm_module->memory.bytes, nullptr);
    EXPECT_EQ(wasm_module->table.entries, nullptr);
}

TEST_F(ModuleFixture, DimmerLoadsWithTableMemoryAndGlobals) {
    warduino->instantiate_module(wasm_module, dimmer_wasm, dimmer_wasm_len);
    ASSERT_NE(wasm_module, nullptr);
    EXPECT_NE(wasm_module->types, nullptr);
    EXPECT_NE(wasm_module->functions, nullptr);
    EXPECT_NE(wasm_module->stack, nullptr);
    EXPECT_NE(wasm_module->callstack, nullptr);
    EXPECT_NE(wasm_module->br_table, nullptr);
    EXPECT_NE(wasm_module->globals, nullptr);
    EXPECT_NE(wasm_module->memory.bytes, nullptr);
    EXPECT_NE(wasm_module->table.entries, nullptr);
}

TEST_F(ModuleFixture, FreeingModuleStateEmptiesModule) {
    warduino->instantiate_module(wasm_module, dimmer_wasm, dimmer_wasm_len);
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

TEST_F(ModuleFixture, FreeingStatePreservesOptions) {
    warduino->instantiate_module(wasm_module, blink_wasm, blink_wasm_len);
    warduino->free_module_state(wasm_module);
    Options opts2 = wasm_module->options;
    EXPECT_EQ(opts.disable_memory_bounds, opts2.disable_memory_bounds);
    EXPECT_EQ(opts.dlsym_trim_underscore, opts2.dlsym_trim_underscore);
    EXPECT_EQ(opts.mangle_table_index, opts2.mangle_table_index);
    EXPECT_EQ(opts.return_exception, opts2.return_exception);
}

TEST_F(ModuleFixture, InstantiatingPreservesRunningState) {
    auto wd = warduino;
    auto mod = wasm_module;
    auto doInstantiate = [wd, mod]() {
        wd->program_state = WARDUINOpause;
        wd->instantiate_module(mod, blink_wasm, blink_wasm_len);
    };

    // async policy may cause an exception
    ASSERT_NO_THROW({
        std::future<void> parsing(
            std::async(std::launch::async, doInstantiate));
        std::future_status status = parsing.wait_for(std::chrono::seconds(1));
        ASSERT_EQ(status, std::future_status::ready);
        ASSERT_EQ(warduino->program_state, WARDUINOpause);
    });
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
