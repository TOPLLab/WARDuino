#ifndef UTIL_H
#define UTIL_H

#include <climits>
#include <cstdint>
#include <cstdlib>
#include <string>

#include "../WARDuino.h"

/**
 * Read arguments for function from binary data.
 *
 * @param type
 * @param data
 * @return The arguments as a list of StackValues. Returns nullptr for arity 0.
 */
StackValue *readArgs(Type function, uint8_t *data);

uint64_t size_leb(uint64_t val);

uint8_t *write_LEB(uint32_t value);

/**
 * Write a Little endian base value.
 * see: https://en.wikipedia.org/wiki/LEB128
 *
 * @param value The value to write.
 * @param buff The buffer to write to.
 * @param size The size of the buffer.
 * @param bits The size of the value type (I32, I64)
 * @param sign Whether the value should be sign-extended.
 * @return The number of bytes written.
 */
uint64_t write_LEB_(uint64_t value, uint8_t *buff, uint32_t size, uint32_t bits,
                    bool sign);

uint64_t write_LEB_32(uint32_t value, uint8_t *buff, uint32_t size);

uint64_t write_LEB_32_signed(uint32_t value, uint8_t *buff, uint32_t size);

/**
 * Read a Little endian base value of 32 bits
 * see: https://en.wikipedia.org/wiki/LEB128
 *
 * @param bytes    from which to decode
 * @param pos      The index in the bytes array to start reading from
 */
uint32_t read_LEB_32(uint8_t **pos);

/**
 * Read a Little endian base value
 * see: https://en.wikipedia.org/wiki/LEB128
 *
 * @param bytes    from which to decode
 * @param pos      The index in the bytes array to start reading from
 * @param maxbits  The maximal numer of bits to read
 */
uint64_t read_LEB(uint8_t **pos, uint32_t maxbits);

/**
 * Read a Little endian base value as signed
 *
 * @param bytes    from which to decode
 * @param pos      The index in the bytes array to start reading from
 * @param maxbits  The maximal numer of bits to read
 */
uint64_t read_LEB_signed(uint8_t **pos, uint32_t maxbits);

uint32_t read_uint32(uint8_t **pos);

/**
 * Read arguments for function from LEB128 data.
 *
 * @param type
 * @param data
 * @return The arguments as a list of StackValues. Returns nullptr for arity 0.
 */
StackValue *readWasmArgs(Type function, uint8_t *data);

/**
 * Deserialise a global value according the binary protocol
 *
 * @param input byte buffer
 * @param value
 * @return True if successful
 */
bool deserialiseStackValue(uint8_t *input, bool decodeType, StackValue *value);

// Parse strings

char *read_string(uint8_t **pos, uint32_t *result_len);

std::string parse_utf8_string(const uint8_t *buffer, uint32_t size,
                              uint32_t offset);

// Byte operations

void sext_8_32(uint32_t *val);

void sext_16_32(uint32_t *val);

void sext_8_64(uint64_t *val);

void sext_16_64(uint64_t *val);

void sext_32_64(uint64_t *val);

uint32_t rotl32(uint32_t n, unsigned int c);

uint32_t rotr32(uint32_t n, unsigned int c);

uint64_t rotl64(uint64_t n, unsigned int c);

uint64_t rotr64(uint64_t n, unsigned int c);

double wa_fmax(double a, double b);

double wa_fmin(double a, double b);

// legacy util functions (todo remove)
uint32_t read_B32(uint8_t **bytes);
uint16_t read_B16(uint8_t **bytes);
int read_B32_signed(uint8_t **bytes);
uint32_t read_L32(uint8_t **bytes);
void chars_as_hexa(unsigned char *dest, unsigned char *source,
                   uint32_t len_source);

unsigned short int sizeof_valuetype(uint32_t);

/**
 * Converts a physical address pointing to a Wasm instruction to a virtual
 * address. Throws FATAL if conversion is not possible
 *
 * @param *physicalAddr pointer to Wasm
 * @param *m pointer to module for which conversation needs to occur
 * @return virtual address
 */
uint32_t toVirtualAddress(uint8_t *physicalAddr, Module *m);

/**
 * The reverse process of toVirtualAddress. Throws FATAL if conversion is not
 * possible
 * @param virtualAddr the virtual address
 * @param *m pointer to module for which conversation needs to occur
 * @return physical address
 */
uint8_t *toPhysicalAddress(uint32_t virtualAddr, Module *m);

/**
 * Predicate that determines whether the conversion of a virtualAddress to a
 * physical address is possible. possible
 * @param virtualAddr the virtual address
 * @param *m pointer to module
 * @return boolean
 */
bool isToPhysicalAddrPossible(uint32_t virtualAddr, Module *m);
#endif
