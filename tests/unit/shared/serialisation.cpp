#include "serialisation.h"

#include <sstream>

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

void Serialiser::encodeToBE16HexString(size_t value, std::string& dest) {
    std::stringstream ss;
    ss << std::hex << std::uppercase << value;

    dest = ss.str();

    // Ensure an even number of hex characters.
    if (dest.length() % 2 != 0) {
        dest.insert(dest.begin(), '0');
    }
}

void Serialiser::encodeB16(uint16_t value, uint8_t* buff) {
    buff[0] = (value >> 8) & 0xFF;
    buff[1] = value & 0xFF;
}

void Serialiser::uint8BufferToHex(const uint8_t* buffer, size_t length,
                                  std::string& output) {
    static constexpr char hex[] = "0123456789ABCDEF";

    output.clear();
    output.reserve(length * 2);

    for (size_t i = 0; i < length; ++i) {
        output.push_back(hex[(buffer[i] >> 4) & 0x0F]);
        output.push_back(hex[buffer[i] & 0x0F]);
    }
}
