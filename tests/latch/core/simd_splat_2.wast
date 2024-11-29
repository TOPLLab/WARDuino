(module
  ;; Accessing lane
  (func (export "as-i8x16_extract_lane_s-operand-first") (param i32) (result i32)
    (i8x16.extract_lane_s 0 (i8x16.splat (local.get 0))))
  (func (export "as-i8x16_extract_lane_s-operand-last") (param i32) (result i32)
    (i8x16.extract_lane_s 15 (i8x16.splat (local.get 0))))
  (func (export "as-i16x8_extract_lane_s-operand-first") (param i32) (result i32)
    (i16x8.extract_lane_s 0 (i16x8.splat (local.get 0))))
  (func (export "as-i16x8_extract_lane_s-operand-last") (param i32) (result i32)
    (i16x8.extract_lane_s 7 (i16x8.splat (local.get 0))))
  (func (export "as-i32x4_extract_lane_s-operand-first") (param i32) (result i32)
    (i32x4.extract_lane 0 (i32x4.splat (local.get 0))))
  (func (export "as-i32x4_extract_lane_s-operand-last") (param i32) (result i32)
    (i32x4.extract_lane 3 (i32x4.splat (local.get 0))))
  (func (export "as-f32x4_extract_lane_s-operand-first") (param f32) (result f32)
    (f32x4.extract_lane 0 (f32x4.splat (local.get 0))))
  (func (export "as-f32x4_extract_lane_s-operand-last") (param f32) (result f32)
    (f32x4.extract_lane 3 (f32x4.splat (local.get 0))))
  (func (export "as-v8x16_swizzle-operands") (param i32) (param i32) (result v128)
    (i8x16.swizzle (i8x16.splat (local.get 0)) (i8x16.splat (local.get 1))))
  (func (export "as-i64x2_extract_lane-operand-first") (param i64) (result i64)
    (i64x2.extract_lane 0 (i64x2.splat (local.get 0))))
  (func (export "as-i64x2_extract_lane-operand-last") (param i64) (result i64)
    (i64x2.extract_lane 1 (i64x2.splat (local.get 0))))
  (func (export "as-f64x2_extract_lane-operand-first") (param f64) (result f64)
    (f64x2.extract_lane 0 (f64x2.splat (local.get 0))))
  (func (export "as-f64x2_extract_lane-operand-last") (param f64) (result f64)
    (f64x2.extract_lane 1 (f64x2.splat (local.get 0))))

  ;; Integer arithmetic
  (func (export "as-i8x16_add_sub-operands") (param i32 i32 i32) (result v128)
    (i8x16.add (i8x16.splat (local.get 0))
      (i8x16.sub (i8x16.splat (local.get 1)) (i8x16.splat (local.get 2)))))
  (func (export "as-i16x8_add_sub_mul-operands") (param i32 i32 i32 i32) (result v128)
    (i16x8.add (i16x8.splat (local.get 0))
      (i16x8.sub (i16x8.splat (local.get 1))
        (i16x8.mul (i16x8.splat (local.get 2)) (i16x8.splat (local.get 3))))))
  (func (export "as-i32x4_add_sub_mul-operands") (param i32 i32 i32 i32) (result v128)
    (i32x4.add (i32x4.splat (local.get 0))
      (i32x4.sub (i32x4.splat (local.get 1))
        (i32x4.mul (i32x4.splat (local.get 2)) (i32x4.splat (local.get 3))))))

  (func (export "as-i64x2_add_sub_mul-operands") (param i64 i64 i64 i64) (result v128)
    (i64x2.add (i64x2.splat (local.get 0))
      (i64x2.sub (i64x2.splat (local.get 1))
        (i64x2.mul (i64x2.splat (local.get 2)) (i64x2.splat (local.get 3))))))
  (func (export "as-f64x2_add_sub_mul-operands") (param f64 f64 f64 f64) (result v128)
    (f64x2.add (f64x2.splat (local.get 0))
      (f64x2.sub (f64x2.splat (local.get 1))
        (f64x2.mul (f64x2.splat (local.get 2)) (f64x2.splat (local.get 3))))))

  ;; Saturating integer arithmetic
  (func (export "as-i8x16_add_sat_s-operands") (param i32 i32) (result v128)
    (i8x16.add_sat_s (i8x16.splat (local.get 0)) (i8x16.splat (local.get 1))))
  (func (export "as-i16x8_add_sat_s-operands") (param i32 i32) (result v128)
    (i16x8.add_sat_s (i16x8.splat (local.get 0)) (i16x8.splat (local.get 1))))
  (func (export "as-i8x16_sub_sat_u-operands") (param i32 i32) (result v128)
    (i8x16.sub_sat_u (i8x16.splat (local.get 0)) (i8x16.splat (local.get 1))))
  (func (export "as-i16x8_sub_sat_u-operands") (param i32 i32) (result v128)
    (i16x8.sub_sat_u (i16x8.splat (local.get 0)) (i16x8.splat (local.get 1))))

  ;; Bit shifts
  (func (export "as-i8x16_shr_s-operand") (param i32 i32) (result v128)
    (i8x16.shr_s (i8x16.splat (local.get 0)) (local.get 1)))
  (func (export "as-i16x8_shr_s-operand") (param i32 i32) (result v128)
    (i16x8.shr_s (i16x8.splat (local.get 0)) (local.get 1)))
  (func (export "as-i32x4_shr_s-operand") (param i32 i32) (result v128)
    (i32x4.shr_s (i32x4.splat (local.get 0)) (local.get 1)))

  ;; Bitwise operantions
  (func (export "as-v128_and-operands") (param i32 i32) (result v128)
    (v128.and (i8x16.splat (local.get 0)) (i8x16.splat (local.get 1))))
  (func (export "as-v128_or-operands") (param i32 i32) (result v128)
    (v128.or (i16x8.splat (local.get 0)) (i16x8.splat (local.get 1))))
  (func (export "as-v128_xor-operands") (param i32 i32) (result v128)
    (v128.xor (i32x4.splat (local.get 0)) (i32x4.splat (local.get 1))))

  ;; Boolean horizontal reductions
  (func (export "as-i8x16_all_true-operand") (param i32) (result i32)
    (i8x16.all_true (i8x16.splat (local.get 0))))
  (func (export "as-i16x8_all_true-operand") (param i32) (result i32)
    (i16x8.all_true (i16x8.splat (local.get 0))))
  (func (export "as-i32x4_all_true-operand1") (param i32) (result i32)
    (i32x4.all_true (i32x4.splat (local.get 0))))
  (func (export "as-i32x4_all_true-operand2") (param i64) (result i32)
    (i32x4.all_true (i64x2.splat (local.get 0))))

  ;; Comparisons
  (func (export "as-i8x16_eq-operands") (param i32 i32) (result v128)
    (i8x16.eq (i8x16.splat (local.get 0)) (i8x16.splat (local.get 1))))
  (func (export "as-i16x8_eq-operands") (param i32 i32) (result v128)
    (i16x8.eq (i16x8.splat (local.get 0)) (i16x8.splat (local.get 1))))
  (func (export "as-i32x4_eq-operands1") (param i32 i32) (result v128)
    (i32x4.eq (i32x4.splat (local.get 0)) (i32x4.splat (local.get 1))))
  (func (export "as-i32x4_eq-operands2") (param i64 i64) (result v128)
    (i32x4.eq (i64x2.splat (local.get 0)) (i64x2.splat (local.get 1))))
  (func (export "as-f32x4_eq-operands") (param f32 f32) (result v128)
    (f32x4.eq (f32x4.splat (local.get 0)) (f32x4.splat (local.get 1))))
  (func (export "as-f64x2_eq-operands") (param f64 f64) (result v128)
    (f64x2.eq (f64x2.splat (local.get 0)) (f64x2.splat (local.get 1))))

  ;; Floating-point sign bit operations
  (func (export "as-f32x4_abs-operand") (param f32) (result v128)
    (f32x4.abs (f32x4.splat (local.get 0))))

  ;; Floating-point min
  (func (export "as-f32x4_min-operands") (param f32 f32) (result v128)
    (f32x4.min (f32x4.splat (local.get 0)) (f32x4.splat (local.get 1))))

  ;; Floating-point arithmetic
  (func (export "as-f32x4_div-operands") (param f32 f32) (result v128)
    (f32x4.div (f32x4.splat (local.get 0)) (f32x4.splat (local.get 1))))

  ;; Conversions
  (func (export "as-f32x4_convert_s_i32x4-operand") (param i32) (result v128)
    (f32x4.convert_i32x4_s (i32x4.splat (local.get 0))))
  (func (export "as-i32x4_trunc_s_f32x4_sat-operand") (param f32) (result v128)
    (i32x4.trunc_sat_f32x4_s (f32x4.splat (local.get 0))))
)