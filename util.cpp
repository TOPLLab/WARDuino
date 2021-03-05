#include "util.h"

#include <math.h>
#include <string.h>

#include "debug.h"

// Little endian base
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
        // Sign extend
        result |= -(1u << shift);
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
    return static_cast<int>(read_LEB_(pos, maxbits, true));
}

uint32_t read_uint32(uint8_t **pos) {
    *pos += 4;
    return ((uint32_t *)(*pos - 4))[0];
}

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
    double c = fmax(a, b);
    if (c == 0 && a == b) {
        return signbit(a) ? b : a;
    }
    return c;
}

double wa_fmin(double a, double b) {
    double c = fmin(a, b);
    if (c == 0 && a == b) {
        return signbit(a) ? a : b;
    }
    return c;
}
