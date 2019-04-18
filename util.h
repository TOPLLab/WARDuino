#ifndef UTIL_H
#define UTIL_H

#include <stdbool.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

/**
 * Endless loop
 */
void end();

/**
 * Read a Little endian base value
 * see: https://en.wikipedia.org/wiki/LEB128
 * 
 * @param bytes    from which to decode
 * @param pos      The index in the bytes array to start reading from
 * @param maxbits  The maximal numer of bits to read
 */
uint64_t read_LEB(uint8_t *bytes, uint32_t *pos, uint32_t maxbits);

/**
 * Read a Little endian base value as signed
 * 
 * @param bytes    from which to decode
 * @param pos      The index in the bytes array to start reading from
 * @param maxbits  The maximal numer of bits to read
 */
uint64_t read_LEB_signed(uint8_t *bytes, uint32_t *pos, uint32_t maxbits);


uint32_t read_uint32(uint8_t *bytes, uint32_t *pos);
char *read_string(uint8_t *bytes, uint32_t *pos, uint32_t *result_len);
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

#define FATAL(...)                                  \
  {                                                 \
    printf("Error(%s:%d): \n", __FILE__, __LINE__); \
    printf(__VA_ARGS__);                            \
    end();                                          \
  }

#define info(...) \
  { printf(__VA_ARGS__); }
#define log(...) ;
#define error(...) ;
#define ASSERT(exp, ...)                                     \
  {                                                          \
    if (!(exp)) {                                            \
      printf("Assert Failed (%s:%d): ", __FILE__, __LINE__); \
      printf(__VA_ARGS__);                                   \
      end();                                                 \
    }                                                        \
  }
#define debug(...)                                   \
  {                                                  \
    printf("DEBUG (%s:%d):\n ", __FILE__, __LINE__); \
    printf(__VA_ARGS__);                             \
  }

#define warn(...) \
  { printf(__VA_ARGS__); }

#endif
