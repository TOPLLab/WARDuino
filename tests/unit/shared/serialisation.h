#pragma once
#include <cstdint>
#include <iostream>

class Serialiser {
   public:
    static void uint32ToHexString(uint32_t value, std::string& dest);

    static void uint8ToHexString(uint8_t value, std::string& dest);

    static uint8_t* encodeUInt32(uint32_t value, uint8_t* dest = nullptr);

    static void encodeToBE16HexString(size_t value, std::string& dest);

    static void encodeB16(uint16_t value, uint8_t* buff);

    static void uint8BufferToHex(const uint8_t* buffer, size_t length,
                                 std::string& output);
};
