(module
  (func (export "i32.no_fold_div_s_mul") (param $x i32) (result i32)
    (i32.mul (i32.div_s (local.get $x) (i32.const 6)) (i32.const 6)))
  (func (export "i32.no_fold_div_u_mul") (param $x i32) (result i32)
    (i32.mul (i32.div_u (local.get $x) (i32.const 6)) (i32.const 6)))

  (func (export "i64.no_fold_div_s_mul") (param $x i64) (result i64)
    (i64.mul (i64.div_s (local.get $x) (i64.const 6)) (i64.const 6)))
  (func (export "i64.no_fold_div_u_mul") (param $x i64) (result i64)
    (i64.mul (i64.div_u (local.get $x) (i64.const 6)) (i64.const 6)))
)

