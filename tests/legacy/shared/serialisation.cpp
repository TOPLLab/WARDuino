#include "serialisation.h"

#include "../../../src/Utils/util.h"

uint8_t* Serialiser::encodeUInt32(uint32_t value, uint8_t* dest) {
    uint8_t* buff = dest == nullptr ? (uint8_t*)malloc(4) : dest;
    buff[0] = static_cast<uint8_t>((value >> 24) & 0xFF);
    buff[1] = static_cast<uint8_t>((value >> 16) & 0xFF);
    buff[2] = static_cast<uint8_t>((value >> 8) & 0xFF);
    buff[3] = static_cast<uint8_t>(value & 0xFF);
    return buff;
}

void Serialiser::uint32ToHexString(uint32_t value, std::string& dest) {
    uint8_t buff[4] = {'\0'};
    Serialiser::encodeUInt32(value, buff);
    char hexa[9] = {'\0'};
    chars_as_hexa((unsigned char*)hexa, buff, 4);
    hexa[8] = '\0';
    dest = hexa;
}

void Serialiser::uint8ToHexString(uint8_t value, std::string& dest) {
    char hexa[3] = {'\0'};
    chars_as_hexa((unsigned char*)hexa, &value, 1);
    hexa[2] = '\0';
    dest = hexa;
}