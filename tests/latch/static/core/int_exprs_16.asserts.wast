(assert_return (invoke "i32.rem_s_5" (i32.const 71)) (i32.const 1))
(assert_return (invoke "i32.rem_s_5" (i32.const 0x50000000)) (i32.const 0))
(assert_return (invoke "i32.rem_u_5" (i32.const 71)) (i32.const 1))
(assert_return (invoke "i32.rem_u_5" (i32.const 0xa0000000)) (i32.const 0))
(assert_return (invoke "i64.rem_s_5" (i64.const 71)) (i64.const 1))
(assert_return (invoke "i64.rem_s_5" (i64.const 0x5000000000000000)) (i64.const 0))
(assert_return (invoke "i64.rem_u_5" (i64.const 71)) (i64.const 1))
(assert_return (invoke "i64.rem_u_5" (i64.const 0xa000000000000000)) (i64.const 0))


