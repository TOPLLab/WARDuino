#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "../../src/Utils/util.h"

TEST_CASE("Test: to virtual address conversion") {
    uint8_t wasm_bytes[] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee};
    Module module{};
    module.bytes = wasm_bytes;
    module.byte_count = sizeof(wasm_bytes);

    SUBCASE("Start of module maps to zero") {
        CHECK(toVirtualAddress(module.bytes, &module) == 0);
    }

    SUBCASE("Middle byte maps to its offset") {
        CHECK(toVirtualAddress(module.bytes + 3, &module) == 3);
    }

    SUBCASE("End pointer maps to byte_count") {
        CHECK(toVirtualAddress(module.bytes + module.byte_count, &module) ==
              module.byte_count);
    }
}

TEST_CASE("Test: to physical address conversion") {
    uint8_t wasm_bytes[] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee};
    Module module{};
    module.bytes = wasm_bytes;
    module.byte_count = sizeof(wasm_bytes);

    SUBCASE("Virtual zero maps to start pointer") {
        CHECK(toPhysicalAddress(0, &module) == module.bytes);
    }

    SUBCASE("Virtual middle offset maps to expected physical pointer") {
        CHECK(toPhysicalAddress(2, &module) == module.bytes + 2);
    }

    SUBCASE("Last valid virtual offset maps to final byte") {
        CHECK(toPhysicalAddress(module.byte_count - 1, &module) ==
              module.bytes + module.byte_count - 1);
    }
}
