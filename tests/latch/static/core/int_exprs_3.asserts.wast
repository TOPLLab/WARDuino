(assert_return (invoke "i32.no_fold_shl_shr_s" (i32.const 0x80000000)) (i32.const 0))
(assert_return (invoke "i32.no_fold_shl_shr_u" (i32.const 0x80000000)) (i32.const 0))
(assert_return (invoke "i64.no_fold_shl_shr_s" (i64.const 0x8000000000000000)) (i64.const 0))
(assert_return (invoke "i64.no_fold_shl_shr_u" (i64.const 0x8000000000000000)) (i64.const 0))


