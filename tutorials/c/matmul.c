//
// Created by jay on 10/10/24.
//
#include "stdint.h"
#include "wasm_simd128.h"

static unsigned char *memory[1024];

__attribute__((import_module("env"), import_name("print_int")))
extern void print_int(int value);

/**
 * Computes a * b + c (lane-wise)
 */
__attribute__((noinline))
v128_t vmlaq_u16(const v128_t a, const v128_t b, const v128_t c) {
  return wasm_i16x8_add(wasm_i16x8_mul(a, b), c);
}

/**
 * Computes (a >> shift) + b (lane-wise)
 */
__attribute__((noinline))
v128_t vsraq_n_u16(const v128_t a, const v128_t b, const unsigned int shift) {
  return wasm_i16x8_add(b, wasm_i16x8_shr(a, shift));
}

/**
 * Computes the sum of all lanes in a vector
 */
__attribute__((noinline))
short vaddvq_u16(v128_t v) {
#define SWIZZLE_MASK0 wasm_i8x16_const(0x4, 0x5, 0x6, 0x7, -1, -1, -1, -1, 0xc, 0xd, 0xe, 0xf, -1, -1, -1, -1)
#define SWIZZLE_MASK1 wasm_i8x16_const(0x8, 0x9, 0xa, 0xb, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1)

  v = wasm_i32x4_extadd_pairwise_i16x8(v);
  v = wasm_i32x4_add(v, wasm_i8x16_swizzle(v, SWIZZLE_MASK0));
  v = wasm_i32x4_add(v, wasm_i8x16_swizzle(v, SWIZZLE_MASK1));
  return wasm_i32x4_extract_lane(v, 0);
}

__attribute__((noinline))
void print_v128(const v128_t v) {
  print_int(wasm_i16x8_extract_lane(v, 0));
  print_int(wasm_i16x8_extract_lane(v, 1));
  print_int(wasm_i16x8_extract_lane(v, 2));
  print_int(wasm_i16x8_extract_lane(v, 3));
  print_int(wasm_i16x8_extract_lane(v, 4));
  print_int(wasm_i16x8_extract_lane(v, 5));
  print_int(wasm_i16x8_extract_lane(v, 6));
  print_int(wasm_i16x8_extract_lane(v, 7));
}

__attribute__((noinline))
v128_t vmovq_n_u16(const unsigned short n) {
  return wasm_i16x8_splat(n);
}

__attribute__((noinline))
v128_t vld1q_u16(const unsigned short *ptr) {
  return wasm_v128_load(ptr);
}

__attribute__((noinline))
v128_t vandq_u16(const v128_t a, const v128_t b) {
  return wasm_v128_and(a, b);
}

unsigned short small_dot(const unsigned short w, const unsigned short a) {
  return ((w * a) & 0x0f00) >> 8;
}

unsigned short small_vec_dot(const v128_t w, const v128_t a) {
  return vaddvq_u16(wasm_i16x8_shr(vandq_u16(wasm_i16x8_mul(w, a), wasm_i16x8_splat(0x0f00)), 8));
}

unsigned short small_vec_dot_ptr(const unsigned short *w, const unsigned short *a) {
  v128_t local = vmovq_n_u16(0);
  v128_t sum = vmovq_n_u16(0);
  const v128_t v_w = vld1q_u16(w);
  const v128_t v_a = vld1q_u16(a);
  local = vmlaq_u16(v_w, v_a, local);
  local = vandq_u16(local, wasm_i16x8_splat(0x0f00));
  sum = vsraq_n_u16(local, sum, 8);
  return vaddvq_u16(sum);
}

__attribute__((noinline))
int dot(const unsigned short *w, const unsigned short *a, const int k) {
  // expect w=1, a=1, k%(depth*iter*8)=0
#define DEPTH 2
#define ITER 2
#define MASK vmovq_n_u16(0x0f00)
  // expect k % 32 == 0

  v128_t sum = vmovq_n_u16(0);
  for(int i = 0; i < k; i += 8 * DEPTH * ITER) {
    v128_t local = vmovq_n_u16(0);
    for(int j = 0; j < ITER; j++) {
      v128_t vecw = vld1q_u16(w);
      v128_t veca = vld1q_u16(a);
      local = vmlaq_u16(vecw, veca, local);
      w += 8;
      a += 8;
    }
    local = vandq_u16(local, MASK);
    sum = vsraq_n_u16(local, sum, 8);
  }
  return vaddvq_u16(sum);
}

__attribute__((export_name("main")))
void _start() {
  const unsigned short packed_w[16] = {
    0x0001, 0x0001, 0x0001, 0x0001, 0x0000, 0x0101, 0x0202, 0x0303,
    0x0302, 0x0100, 0x0201, 0x0001, 0x0201, 0x0201, 0x0201, 0x0201
  };

  const unsigned short packed_a[16] = {
    0x0100, 0x0002, 0x0201, 0x0100, 0x0100, 0x0200, 0x0300, 0x0000,
    0x0100, 0x0200, 0x0300, 0x0400, 0x0001, 0x0001, 0x0001, 0x0001
  };

  print_int(dot(packed_w, packed_a, 32)); // OK!

  // print_int(small_dot(0x0302, 0x0104)); // OK!

  // print_int(small_vec_dot(
  //   wasm_i16x8_const(0x0001, 0x0203, 0x0001, 0x0203, 0x0302, 0x0100, 0x0302, 0x0100),
  //   wasm_i16x8_const(0x0001, 0x0001, 0x0001, 0x0001, 0x0201, 0x0201, 0x0201, 0x0201)
  // )); // OK!

  // const unsigned short w[16] = {
  //   0x0001, 0x0203, 0x0001, 0x0203, 0x0302, 0x0100, 0x0302, 0x0100
  // };
  // const unsigned short a[16] = {
  //   0x0001, 0x0001, 0x0001, 0x0001, 0x0201, 0x0201, 0x0201, 0x0201
  // };
  // print_int(small_vec_dot_ptr(w, a)); // OK!
}