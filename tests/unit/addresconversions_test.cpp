#include "../../src/Utils/util.h"
#include "example_code/fac/fac_wasm.h"
#include "gtest/gtest.h"

class ConversionFixture : public ::testing::Test {
   protected:
    WARDuino* warduino;
    Module* wasm_module;
    Options opts;

    ConversionFixture() : warduino(WARDuino::instance()) {}

    ~ConversionFixture() override {}

    void SetUp() override {
        this->wasm_module = new Module;
        opts = {.disable_memory_bounds = false,
                .mangle_table_index = false,
                .dlsym_trim_underscore = false,
                .return_exception = true};
        wasm_module->options = opts;
        wasm_module->warduino = this->warduino;
        warduino->instantiate_module(wasm_module, fac_wasm, fac_wasm_len);
    }

    void TearDown() override {
        wasm_module->warduino = nullptr;
        warduino->shutdown();
        this->wasm_module = nullptr;
        delete wasm_module;
    }
};

using ConversionDeathTest = ConversionFixture;

TEST_F(ConversionDeathTest, ExitWhenPhysAddrIsSmallerThanStartWasmPhysAddr) {
    uint8_t* startWasmPhysAddr = this->wasm_module->bytes;
    uint8_t* smallerInvalidAddr =
        startWasmPhysAddr - 1;  // randomly chosen number

    auto expectedStdErrMsg = "";
    EXPECT_EXIT(toVirtualAddress(smallerInvalidAddr, this->wasm_module),
                ::testing::ExitedWithCode(1), expectedStdErrMsg);
}

TEST_F(ConversionDeathTest, ExitWhenPhysAddrIsBiggerThanEndWasmPhysAddr) {
    uint8_t* startWasmPhysAddr = this->wasm_module->bytes;
    uint8_t* biggerInvalidAddr = startWasmPhysAddr +
                                 this->wasm_module->byte_count +
                                 1;  // randomly chosen number
    auto expectedStdErrMsg = "";
    EXPECT_EXIT(toVirtualAddress(biggerInvalidAddr, this->wasm_module),
                ::testing::ExitedWithCode(1), expectedStdErrMsg);
}

TEST_F(ConversionDeathTest, FromInvalidVirtAddrToPhysAddrThrowsFatal) {
    uint32_t wasm_size = this->wasm_module->byte_count;
    uint32_t invalidAddr = wasm_size + 20;  // randomly chosen number

    auto expectedStdErrMsg = "";
    EXPECT_EXIT(toPhysicalAddress(invalidAddr, this->wasm_module),
                ::testing::ExitedWithCode(1), expectedStdErrMsg);
}

TEST_F(ConversionFixture, PredicateForInvalidPhyAddrConversionReturnsFalse) {
    uint32_t wasm_size = this->wasm_module->byte_count;
    uint32_t invalidAddr = wasm_size + 20;  // randomly chosen number
    ASSERT_FALSE(isToPhysicalAddrPossible(invalidAddr, this->wasm_module));
}

TEST_F(ConversionFixture,
       PredicateForMinimalValidPhyAddrConversionReturnsTrue) {
    uint32_t minimalVirAddr = 0;
    ASSERT_TRUE(isToPhysicalAddrPossible(minimalVirAddr, this->wasm_module));
}

TEST_F(ConversionFixture,
       PredicateForMaximalValidPhyAddrConversionReturnsTrue) {
    uint32_t maximalVirAddr = this->wasm_module->byte_count - 1;
    ASSERT_TRUE(isToPhysicalAddrPossible(maximalVirAddr, this->wasm_module));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
