(module
  (func (export "i8x16.splat") (param i32) (result v128) (i8x16.splat (local.get 0)))
  (func (export "i16x8.splat") (param i32) (result v128) (i16x8.splat (local.get 0)))
  (func (export "i32x4.splat") (param i32) (result v128) (i32x4.splat (local.get 0)))
  (func (export "f32x4.splat") (param f32) (result v128) (f32x4.splat (local.get 0)))
  (func (export "i64x2.splat") (param i64) (result v128) (i64x2.splat (local.get 0)))
  (func (export "f64x2.splat") (param f64) (result v128) (f64x2.splat (local.get 0)))
)