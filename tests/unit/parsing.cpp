#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <cmath>
#include <vector>

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

namespace {

std::vector<uint8_t> encode_rle_for_test(const std::vector<uint8_t> &input) {
    std::vector<uint8_t> encoded;
    for (size_t position = 0; position < input.size();) {
        const uint8_t value = input[position];
        size_t count = 1;
        while (++position < input.size() && input[position] == value) ++count;

        do {
            uint8_t byte = count & 0x7fU;
            count >>= 7U;
            if (count != 0) byte |= 0x80U;
            encoded.push_back(byte);
        } while (count != 0);
        encoded.push_back(value);
    }
    return encoded;
}

}  // namespace

TEST_CASE("Test: byte RLE run formation and exact decoding") {
    SUBCASE("Empty memory has an empty representation") {
        std::vector<uint8_t> decoded{1, 2, 3};
        CHECK(decode_rle_exact(nullptr, 0, 0, &decoded));
        CHECK(decoded.empty());
    }

    SUBCASE("A zero page is one legacy-compatible run") {
        std::vector<uint8_t> page(PAGE_SIZE, 0);
        const std::vector<uint8_t> encoded = encode_rle_for_test(page);
        CHECK(encoded == std::vector<uint8_t>{0x80, 0x80, 0x04, 0x00});

        std::vector<uint8_t> decoded;
        CHECK(decode_rle_exact(encoded.data(), encoded.size(), page.size(),
                               &decoded));
        CHECK(decoded == page);
    }

    SUBCASE("Mixed and alternating runs retain their order") {
        const std::vector<uint8_t> mixed{4, 4, 4, 9, 9, 2, 7, 7, 7, 7};
        const std::vector<uint8_t> alternating{0, 1, 0, 1, 0, 1};
        for (const auto *input : {&mixed, &alternating}) {
            const std::vector<uint8_t> encoded = encode_rle_for_test(*input);
            std::vector<uint8_t> decoded;
            CHECK(decode_rle_exact(encoded.data(), encoded.size(),
                                   input->size(), &decoded));
            CHECK(decoded == *input);
        }
        CHECK(encode_rle_for_test(alternating).size() ==
              alternating.size() * 2);
    }
}

TEST_CASE("Test: malformed byte RLE is rejected atomically") {
    const std::vector<uint8_t> original{8, 8};
    std::vector<uint8_t> decoded = original;

    const std::vector<std::vector<uint8_t>> malformed{
        {0x00, 0x2a},
        {0x80},
        {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00},
        {0x01},
        {0x03, 0x2a},
        {0x01, 0x2a},
    };

    for (const auto &encoded : malformed) {
        CAPTURE(encoded);
        CHECK_FALSE(
            decode_rle_exact(encoded.data(), encoded.size(), 2, &decoded));
        CHECK(decoded == original);
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