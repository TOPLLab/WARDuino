(assert_return (invoke "i32.no_fold_shr_s_shl" (i32.const 1)) (i32.const 0))
(assert_return (invoke "i32.no_fold_shr_u_shl" (i32.const 1)) (i32.const 0))
(assert_return (invoke "i64.no_fold_shr_s_shl" (i64.const 1)) (i64.const 0))
(assert_return (invoke "i64.no_fold_shr_u_shl" (i64.const 1)) (i64.const 0))


