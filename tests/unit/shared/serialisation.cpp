#include "serialisation.h"

#include <iomanip>
#include <iostream>
#include <sstream>

std::string Serialiser::uint32ToHexString(uint32_t value) {
    std::ostringstream oss;
    oss << std::hex << std::setw(8) << std::setfill('0') << value;
    return oss.str();
}