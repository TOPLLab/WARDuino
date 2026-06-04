#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

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