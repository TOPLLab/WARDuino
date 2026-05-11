(assert_return (invoke "i32.no_fold_div_s_mul" (i32.const 1)) (i32.const 0))
(assert_return (invoke "i32.no_fold_div_u_mul" (i32.const 1)) (i32.const 0))
(assert_return (invoke "i64.no_fold_div_s_mul" (i64.const 1)) (i64.const 0))
(assert_return (invoke "i64.no_fold_div_u_mul" (i64.const 1)) (i64.const 0))


