(module
  (func (export "i32.rem_s_5") (param $x i32) (result i32)
    (i32.rem_s (local.get $x) (i32.const 5)))
  (func (export "i32.rem_u_5") (param $x i32) (result i32)
    (i32.rem_u (local.get $x) (i32.const 5)))

  (func (export "i64.rem_s_5") (param $x i64) (result i64)
    (i64.rem_s (local.get $x) (i64.const 5)))
  (func (export "i64.rem_u_5") (param $x i64) (result i64)
    (i64.rem_u (local.get $x) (i64.const 5)))
)

