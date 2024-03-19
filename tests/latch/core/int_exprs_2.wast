(module
  (func (export "i64.no_fold_wrap_extend_u") (param $x i64) (result i64)
    (i64.extend_i32_u (i32.wrap_i64 (local.get $x))))
)

