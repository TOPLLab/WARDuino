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

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
