(module
  (func (export "i32.no_fold_shr_s_shl") (param $x i32) (result i32)
    (i32.shl (i32.shr_s (local.get $x) (i32.const 1)) (i32.const 1)))
  (func (export "i32.no_fold_shr_u_shl") (param $x i32) (result i32)
    (i32.shl (i32.shr_u (local.get $x) (i32.const 1)) (i32.const 1)))

  (func (export "i64.no_fold_shr_s_shl") (param $x i64) (result i64)
    (i64.shl (i64.shr_s (local.get $x) (i64.const 1)) (i64.const 1)))
  (func (export "i64.no_fold_shr_u_shl") (param $x i64) (result i64)
    (i64.shl (i64.shr_u (local.get $x) (i64.const 1)) (i64.const 1)))
)

