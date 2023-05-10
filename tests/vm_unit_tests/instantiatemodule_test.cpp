#include <future>

#include "../../src/WARDuino.h"
#include "example_code/blink/blink_wasm.h"
#include "example_code/dimmer/dimmer_wasm.h"
#include "example_code/fac/fac_wasm.h"
#include "gtest/gtest.h"

class InstantiateModuleFixture : public ::testing::Test {
   protected:
    WARDuino* warduino;
    Module* wasm_module;
    Options opts;

    InstantiateModuleFixture() : warduino(WARDuino::instance()) {}

    ~InstantiateModuleFixture() override {}

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

TEST_F(InstantiateModuleFixture, InitiallyEmpty) {
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

TEST_F(InstantiateModuleFixture, FacLoadsWithoutTableGlobalsAndMemory) {
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

TEST_F(InstantiateModuleFixture, BlinkLoadsWithoutTableAndMemory) {
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

TEST_F(InstantiateModuleFixture, DimmerLoadsWithTableMemoryAndGlobals) {
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

TEST_F(InstantiateModuleFixture, InstantiatingWorksDespitePauseState) {
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

TEST_F(InstantiateModuleFixture, InstantiatingWorksDespiteStepState) {
    auto wd = warduino;
    auto mod = wasm_module;
    auto doInstantiate = [wd, mod]() {
        wd->program_state = WARDUINOstep;
        wd->instantiate_module(mod, blink_wasm, blink_wasm_len);
    };

    // async policy may cause an exception
    ASSERT_NO_THROW({
        std::future<void> parsing(
            std::async(std::launch::async, doInstantiate));
        std::future_status status = parsing.wait_for(std::chrono::seconds(1));
        ASSERT_EQ(status, std::future_status::ready);
        ASSERT_EQ(warduino->program_state, WARDUINOstep);
    });
}

TEST_F(InstantiateModuleFixture, InstantiatingWorksDespitePROXYrunState) {
    auto wd = warduino;
    auto mod = wasm_module;
    auto doInstantiate = [wd, mod]() {
        wd->program_state = PROXYrun;
        wd->instantiate_module(mod, blink_wasm, blink_wasm_len);
    };

    // async policy may cause an exception
    ASSERT_NO_THROW({
        std::future<void> parsing(
            std::async(std::launch::async, doInstantiate));
        std::future_status status = parsing.wait_for(std::chrono::seconds(1));
        ASSERT_EQ(status, std::future_status::ready);
        ASSERT_EQ(warduino->program_state, PROXYrun);
    });
}

TEST_F(InstantiateModuleFixture, InstantiatingWorksDespitePROXYhaltState) {
    auto wd = warduino;
    auto mod = wasm_module;
    auto doInstantiate = [wd, mod]() {
        wd->program_state = PROXYhalt;
        wd->instantiate_module(mod, blink_wasm, blink_wasm_len);
    };

    // async policy may cause an exception
    ASSERT_NO_THROW({
        std::future<void> parsing(
            std::async(std::launch::async, doInstantiate));
        std::future_status status = parsing.wait_for(std::chrono::seconds(1));
        ASSERT_EQ(status, std::future_status::ready);
        ASSERT_EQ(warduino->program_state, PROXYhalt);
    });
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
