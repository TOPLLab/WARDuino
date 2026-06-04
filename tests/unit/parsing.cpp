#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <cmath>

#include "../../src/Utils/util.h"

TEST_CASE("Test: leb128 unsigned encoding") {
    SUBCASE("Decodes single-byte values") {
        uint8_t bytes[] = {0x00, 0x7f};
        uint8_t *pos = bytes;

        CHECK(read_LEB(&pos, 32) == 0);
        CHECK(pos == bytes + 1);

        CHECK(read_LEB(&pos, 32) == 127);
        CHECK(pos == bytes + 2);
    }

    SUBCASE("Decodes multi-byte values") {
        uint8_t bytes[] = {0x80, 0x01, 0xe5, 0x8e, 0x26};
        uint8_t *pos = bytes;

        CHECK(read_LEB(&pos, 32) == 128);
        CHECK(pos == bytes + 2);

        // Example from the LEB128 specification: 624485 -> E5 8E 26
        CHECK(read_LEB(&pos, 32) == 624485);
        CHECK(pos == bytes + 5);
    }
}

TEST_CASE("Test: leb128 signed encoding") {
    SUBCASE("Decodes single-byte values") {
        uint8_t bytes[] = {0x00, 0x7f};
        uint8_t *pos = bytes;

        CHECK(read_LEB_signed(&pos, 32) == 0);
        CHECK(pos == bytes + 1);

        CHECK(read_LEB_signed(&pos, 32) == -1);
        CHECK(pos == bytes + 2);
    }
}


TEST_CASE("Test: wasm args parsing") {
    uint32_t params[] = {F32, F32};
    uint32_t results[] = {I32};
    Type function_type = {FUNC, 2, params, 1, results, 0};

    // 01 00 80 7f -> NaN (0x7f800001), 00 00 80 7f -> +Infinity
    uint8_t data[] = {0x01, 0x00, 0x80, 0x7f, 0x00, 0x00, 0x80, 0x7f};

    StackValue *args = readWasmArgs(function_type, data);

    REQUIRE(args != nullptr);
    CHECK(args[0].value_type == F32);
    CHECK(std::isnan(args[0].value.f32));

    CHECK(args[1].value_type == F32);
    CHECK(std::isinf(args[1].value.f32));

    delete[] args;
}