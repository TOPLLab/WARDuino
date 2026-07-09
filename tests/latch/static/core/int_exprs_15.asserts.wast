(assert_return (invoke "i32.rem_s_3" (i32.const 71)) (i32.const 2))
(assert_return (invoke "i32.rem_s_3" (i32.const 0x60000000)) (i32.const 0))
(assert_return (invoke "i32.rem_u_3" (i32.const 71)) (i32.const 2))
(assert_return (invoke "i32.rem_u_3" (i32.const 0xc0000000)) (i32.const 0))
(assert_return (invoke "i64.rem_s_3" (i64.const 71)) (i64.const 2))
(assert_return (invoke "i64.rem_s_3" (i64.const 0x3000000000000000)) (i64.const 0))
(assert_return (invoke "i64.rem_u_3" (i64.const 71)) (i64.const 2))
(assert_return (invoke "i64.rem_u_3" (i64.const 0xc000000000000000)) (i64.const 0))


