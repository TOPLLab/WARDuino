#pragma once
#include <iostream>

class Serialiser {
   public:
    static void uint32ToHexString(uint32_t value, std::string& dest);

    static void uint8ToHexString(uint8_t value, std::string& dest);

    static uint8_t* encodeUInt32(uint32_t value, uint8_t* dest = nullptr);
};