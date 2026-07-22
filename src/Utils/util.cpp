#include "util.h"

#include <cmath>
#include <cstring>

#include "macros.h"

StackValue *readArgs(Type function, uint8_t *data) {
    auto *args = new StackValue[function.param_count];
    for (uint32_t i = 0; i < function.param_count; i++) {
        args[i] = {static_cast<uint8_t>(function.params[i]), {0}};

        switch (args[i].value_type) {
            case I32: {
                memcpy(&args[i].value.uint32, data, sizeof(uint32_t));
                data += sizeof(uint32_t);
                break;
            }
            case F32: {
                memcpy(&args[i].value.f32, data, sizeof(float));
                data += sizeof(float);
                break;
            }
            case I64: {
                memcpy(&args[i].value.uint64, data, sizeof(uint64_t));
                data += sizeof(uint64_t);
                break;
            }
            case F64: {
                memcpy(&args[i].value.f64, data, sizeof(double));
                data += sizeof(double);
                break;
            }
            default: {
                FATAL("no argument of type %" SCNu8 "\n", args[i].value_type);
            }
        }
    }
    return args;
}

// Little endian base (LED128)

uint64_t read_LEB_(uint8_t **pos, uint32_t maxbits, bool sign) {
    uint64_t result = 0;
    uint32_t shift = 0;
    uint32_t bcnt = 0;
    uint8_t *startpos = *pos;
    uint64_t byte;

    while (true) {
        byte = (uint64_t) * *pos;
        *pos += 1;
        result |= ((byte & 0x7fu) << shift);
        shift += 7;
        if ((byte & 0x80u) == 0) {
            break;
        }
        bcnt += 1;
        if (bcnt > (maxbits + 7 - 1) / 7) {
            FATAL("Unsigned LEB at byte %p overflow", (void *)startpos);
        }
    }
    if (sign && (shift < maxbits) && (byte & 0x40u)) {
        // Sign extend by highest bits set 1 except last shift bits
        result |= UINT64_MAX << shift;
    }
    return result;
}

uint32_t read_LEB_32(uint8_t **pos) {
    return static_cast<uint32_t>(read_LEB(pos, 32));
}

uint64_t read_LEB(uint8_t **pos, uint32_t maxbits) {
    return read_LEB_(pos, maxbits, false);
}

uint64_t read_LEB_signed(uint8_t **pos, uint32_t maxbits) {
    return read_LEB_(pos, maxbits, true);
}

uint32_t read_uint32(uint8_t **pos) {
    *pos += 4;
    return ((uint32_t *)(*pos - 4))[0];
}

StackValue *readWasmArgs(Type function, uint8_t *data) {
    auto *args = new StackValue[function.param_count];
    for (uint32_t i = 0; i < function.param_count; i++) {
        args[i] = {static_cast<uint8_t>(function.params[i]), {0}};

        switch (args[i].value_type) {
            case I32: {
                args[i].value.int32 = read_LEB_signed(&data, 32);
                break;
            }
            case F32: {
                memcpy(&args[i].value.f32, data,
                       sizeof(float));  // todo read ieee 754
                data += sizeof(float);
                break;
            }
            case I64: {
                args[i].value.int64 = read_LEB_signed(&data, 64);
                break;
            }
            case F64: {
                memcpy(&args[i].value.f64, data, sizeof(double));
                data += sizeof(double);
                break;
            }
            default: {
                FATAL("no argument of type %" SCNu8 "\n", args[i].value_type);
            }
        }
    }
    return args;
}

bool deserialiseStackValue(uint8_t *input, bool decodeType, StackValue *value) {
    if (decodeType) {
        uint8_t valtypes[] = {I32, I64, F32, F64};
        uint8_t type_index = *input++;
        if (type_index >= sizeof(valtypes)) return false;
        value->value_type = valtypes[type_index];
    }
    value->value.uint64 = 0;  // init whole union to 0
    switch (value->value_type) {
        case I32:
            // TODO figure out whether read_LEB_signed or read_LEB is needed
            value->value.uint32 = read_LEB_signed(&input, 32);
            break;
        case I64:
            // TODO figure out whether read_LEB_signed or read_LEB is needed
            value->value.int64 = read_LEB_signed(&input, 64);
            break;
        case F32:
            memcpy(&value->value.uint32, input, 4);
            break;
        case F64:
            memcpy(&value->value.uint64, input, 8);
            break;
        default:
            return false;
    }
    return true;
}

// Strings

// Reads a string from the bytes array at pos that starts with a LEB length
// if result_len is not NULL, then it will be set to the string length
char *read_string(uint8_t **pos, uint32_t *result_len) {
    uint32_t str_len = read_LEB_32(pos);
    char *str = (char *)malloc(str_len + 1);
    memcpy(str, *pos, str_len);
    str[str_len] = '\0';
    *pos += str_len;
    if (result_len) {
        *result_len = str_len;
    }
    return str;
}

std::string parse_utf8_string(const uint8_t *buffer, uint32_t size,
                              uint32_t offset) {
    std::string str;
    size += offset;
    while (offset < size) {
        str += buffer[offset];
        offset++;
    }
    return str;
}

// Math

// Inplace sign extend
void sext_8_32(uint32_t *val) {
    if (*val & 0x80u) {
        *val = *val | 0xffffff00;
    }
}

void sext_16_32(uint32_t *val) {
    if (*val & 0x8000u) {
        *val = *val | 0xffff0000;
    }
}

void sext_8_64(uint64_t *val) {
    if (*val & 0x80u) {
        *val = *val | 0xffffffffffffff00;
    }
}

void sext_16_64(uint64_t *val) {
    if (*val & 0x8000u) {
        *val = *val | 0xffffffffffff0000;
    }
}

void sext_32_64(uint64_t *val) {
    if (*val & 0x80000000) {
        *val = *val | 0xffffffff00000000;
    }
}

// Based on: http://stackoverflow.com/a/776523/471795
uint32_t rotl32(uint32_t n, unsigned int c) {
    const unsigned int mask = (CHAR_BIT * sizeof(n) - 1);
    c = c % 32;
    c &= mask;
    return (n << c) | (n >> ((-c) & mask));
}

uint32_t rotr32(uint32_t n, unsigned int c) {
    const unsigned int mask = (CHAR_BIT * sizeof(n) - 1);
    c = c % 32;
    c &= mask;
    return (n >> c) | (n << ((-c) & mask));
}

uint64_t rotl64(uint64_t n, unsigned int c) {
    const unsigned int mask = (CHAR_BIT * sizeof(n) - 1);
    c = c % 64;
    c &= mask;
    return (n << c) | (n >> ((-c) & mask));
}

uint64_t rotr64(uint64_t n, unsigned int c) {
    const unsigned int mask = (CHAR_BIT * sizeof(n) - 1);
    c = c % 64;
    c &= mask;
    return (n >> c) | (n << ((-c) & mask));
}

double wa_fmax(double a, double b) {
    if (std::isnan(a) || std::isnan(b)) {
        return NAN;
    }
    double c = fmax(a, b);
    if (c == 0 && a == b) {
        return std::signbit(a) ? b : a;
    }
    return c;
}

double wa_fmin(double a, double b) {
    if (std::isnan(a) || std::isnan(b)) {
        return NAN;
    }
    double c = fmin(a, b);
    if (c == 0 && a == b) {
        return std::signbit(a) ? a : b;
    }
    return c;
}

// WOOD
uint32_t read_B32(uint8_t **bytes) {
    uint8_t *b = *bytes;
    uint32_t n = (b[0] << 24) + (b[1] << 16) + (b[2] << 8) + b[3];
    *bytes += 4;
    return n;
}

uint16_t read_B16(uint8_t **bytes) {
    uint8_t *b = *bytes;
    uint32_t n = (b[0] << 8) + b[1];
    *bytes += 2;
    return n;
}

int read_B32_signed(uint8_t **bytes) {
    uint8_t *b = *bytes;
    int n = (b[0] << 24) + (b[1] << 16) + (b[2] << 8) + b[3];
    *bytes += 4;
    return n;
}  // TODO replace with read_LEB_32? If keep Big endian use memcpy?

uint32_t read_L32(uint8_t **bytes) {
    // uint8_t *b = *bytes;
    uint32_t n = 0;
    memcpy(&n, *bytes, sizeof(uint32_t));
    *bytes += 4;
    return n;
}  // TODO replace with read_LEB_32? If keep Big endian use memcpy?

void chars_as_hexa(unsigned char *dest, unsigned char *source,
                   uint32_t len_source) {
    for (uint32_t i = 0; i < len_source; i++) {
        unsigned c = source[i] >> 4;
        unsigned c2 = source[i] & 0xF;
        dest[i * 2 + 0] = c > 9 ? (c - 10 + 'A') : (c + '0');
        dest[i * 2 + 1] = c2 > 9 ? (c2 - 10 + 'A') : (c2 + '0');
    }
}

unsigned short int sizeof_valuetype(uint32_t vt) {
    switch (vt) {
        case I32:
            return 4;
        case I64:
            return 8;
        case F32:
            return sizeof(float);
        default:
            return sizeof(double);
    }
}

bool is_hexa_char(char c) {
    switch (c) {
        case '0' ... '9':
        case 'A' ... 'F':
        case 'a' ... 'f':
            return true;
        default:
            return false;
    }
}

uint32_t toVirtualAddress(uint8_t *physicalAddr, Module *m) {
    if (physicalAddr - m->bytes < 0 ||
        physicalAddr > m->bytes + m->byte_count) {
        FATAL(
            "INVALID toVirtualAddress conversion: physicalAddr=%p "
            "WasmPhysicalAddr=%p "
            "(Virtual address = %d)",
            (void *)physicalAddr, (void *)m->bytes,
            (int)(physicalAddr - m->bytes));
    }
    return physicalAddr - m->bytes;
}

uint8_t *toPhysicalAddress(uint32_t virtualAddr, Module *m) {
    if (virtualAddr >= m->byte_count) {
        FATAL(
            "INVALID toPhysicalAddress conversion: "
            "VirtualAddr=%" PRIu32 " is not within the Wasm size %" PRIu32 "\n",
            virtualAddr, m->byte_count)
    }
    return m->bytes + virtualAddr;
}

bool isToPhysicalAddrPossible(uint32_t virtualAddr, Module *m) {
    return virtualAddr < m->byte_count;
}

// Function to write a 32-bit integer in LEB32 format into a buffer
size_t write_32BIT_LEB(uint32_t value, uint8_t *buffer) {
    size_t bytesWritten = 0;
    do {
        uint8_t byte = value & 0x7F;  // Extract the least significant 7 bits
        value >>= 7;
        if (value != 0) {
            byte |= 0x80;  // Set the high bit to indicate more bytes
        }
        if (buffer != nullptr) {
            buffer[bytesWritten] = byte;
        }
        bytesWritten++;
    } while (value != 0);

    return bytesWritten;
}

size_t size_for_32BIT_LEB(uint32_t value) {
    return write_32BIT_LEB(value, nullptr);
}
// Function to encode a uint64_t value to LEB128 format

size_t write_64BIT_LEB(uint64_t value, uint8_t *buffer) {
    size_t bytesWritten = 0;
    do {
        uint8_t byte = value & 0x7F;  // Extract the least significant 7 bits
        value >>= 7;
        if (value != 0) {
            byte |= 0x80;  // Set the high bit to indicate more bytes
        }
        if (buffer != nullptr) {
            buffer[bytesWritten] = byte;
        }
        bytesWritten++;
    } while (value != 0);

    return bytesWritten;
}

size_t size_for_64BIT_LEB(uint64_t value) {
    return write_64BIT_LEB(value, nullptr);
}

uint8_t *findStartOfLEB128(uint8_t *ptr) {
    while ((*ptr & 0x80) != 0) {
        // If MSB is set, move the pointer backward
        ptr--;
    }
    return ptr;
}

size_t write_float(float value, uint8_t *buffer) {
    uint32_t *intPtr = (uint32_t *)&value;
    size_t bytes_written{};
    for (bytes_written = 0; bytes_written < sizeof(float); bytes_written++) {
        buffer[bytes_written] =
            (uint8_t)((*intPtr >> (8 * bytes_written)) & 0xFF);
    }
    return bytes_written;
}

float read_float(uint8_t **buffer) {
    uint32_t intValue = 0;
    for (int i = 0; i < sizeof(float); i++) {
        intValue |= (uint32_t) * *buffer << (8 * i);
        *buffer += 1;
    }

    float *floatPtr = (float *)&intValue;
    return *floatPtr;
}

size_t size_for_float(float v) { return sizeof(float); }

bool uint8_to_hex(const uint8_t *data, size_t size, HexUInt8Encoding *dest) {
    if (data == nullptr || size == 0 || dest == nullptr) {
        return false;
    }

    size_t hex_size = size * 2;
    if (dest->encoding == nullptr) {
        dest->encoding =
            (char *)malloc(hex_size + 1);  // +1 for null termination
        if (dest->encoding == nullptr) {
            return false;
        }
    }

    // Convert each byte to a hexadecimal representation
    for (size_t i = 0; i < size; i++) {
        sprintf(dest->encoding + (i * 2), "%02X", data[i]);
    }

    // Add a null terminator to the end of the hex buffer
    dest->encoding[hex_size] = '\0';
    dest->bytesWritten = hex_size;

    return dest;
}

size_t serializeStackValueSize(const StackValue *value,
                               const ValueSerializationConfig &config) {
    size_t size = 0;
    if (config.includeType) {
        size += 1;
    }
    if (config.includeIndex) {
        FATAL("Index serialization not yet supported\n");
    }
    switch (value->value_type) {
        case I32:
            size += size_for_32BIT_LEB(value->value.uint32);
            break;
        case I64:
            size += size_for_64BIT_LEB(value->value.uint64);
            break;
        case F32:
            size += size_for_float(value->value.f32);
            break;
        case F64:
            FATAL("Not yet supported\n");
    }
    return size;
}

size_t serializeStackValue(const StackValue &value,
                           const ValueSerializationConfig &config,
                           uint8_t *buffer) {
    size_t offset = 0;
    if (config.includeType) {
        buffer[offset++] = value.value_type;
    }

    if (config.includeIndex) {
        FATAL("Index serialization not yet supported\n");
    }
    switch (value.value_type) {
        case I32:
            offset += write_32BIT_LEB(value.value.uint32, buffer + offset);
            break;
        case I64:
            offset += write_64BIT_LEB(value.value.uint64, buffer + offset);
            break;
        case F32:
            offset += write_float(value.value.f32, buffer + offset);
            break;
        case F64:
            FATAL("Not yet supported\n");
    }
    return offset;
}

size_t deserializeStackValue(StackValue *value,
                             const ValueDeserializationConfig &config,
                             uint8_t *buffer, uint8_t value_type) {
    if (value == nullptr) {
        FATAL("Provided StackValue is a nullpointer\n");
    }

    uint8_t *data = buffer;
    if (config.includeType) {
        value_type = *data++;
    }
    if (config.includeIndex) {
        FATAL("Index serialization not yet supported\n");
    }

    value->value.uint64 = 0;  // init all possible values to 0
    switch (value_type) {
        case I32:
            // TODO figure out whether read_LEB_signed or read_LEB is needed
            value->value.int32 = read_LEB(&data, 32);
            break;
        case I64:
            // TODO figure out whether read_LEB_signed or read_LEB is needed
            value->value.int64 = read_LEB(&data, 64);
            break;
        case F32: {
            value->value.f32 = read_float(&data);
            break;
        }
        case F64:
            FATAL("F64 Not yet supported\n");
            break;
        default:
            FATAL("Unknown type\n");
            break;
    }
    value->value_type = value_type;

    size_t bytesRead = data - buffer;
    return bytesRead;
}

size_t serializeStackValues(const StackValue *vals, uint32_t nr_vals,
                            const ValueSerializationConfig &config,
                            uint8_t *buffer) {
    size_t offset = write_32BIT_LEB(nr_vals, buffer);
    for (auto i = 0; i < nr_vals; ++i) {
        offset += serializeStackValue(vals[i], config, buffer + offset);
    }
    return offset;
}

size_t size_for_stackvalues(StackValue *val, uint32_t nr_vals,
                            const ValueSerializationConfig &config) {
    size_t total_size = size_for_32BIT_LEB(nr_vals);
    for (auto i = 0; i < nr_vals; ++i) {
        total_size += serializeStackValueSize(&val[i], config);
    }
    return total_size;
}

StackValue *deserializeStackValues(uint8_t *encoded_data,
                                   const ValueDeserializationConfig &config,
                                   Type *type) {
    uint32_t nr_args = read_LEB_32(&encoded_data);
    StackValue *values = new StackValue[nr_args];
    size_t bytes_read = 0;
    for (auto i = 0; i < nr_args; ++i) {
        uint8_t value_type = type == nullptr ? 0 : type->params[i];
        bytes_read += deserializeStackValue(
            &values[i], config, encoded_data + bytes_read, value_type);
    }
    return values;
}
