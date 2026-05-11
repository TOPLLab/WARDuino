(assert_return (invoke "i32.rem_s_7" (i32.const 71)) (i32.const 1))
(assert_return (invoke "i32.rem_s_7" (i32.const 0x70000000)) (i32.const 0))
(assert_return (invoke "i32.rem_u_7" (i32.const 71)) (i32.const 1))
(assert_return (invoke "i32.rem_u_7" (i32.const 0xe0000000)) (i32.const 0))
(assert_return (invoke "i64.rem_s_7" (i64.const 71)) (i64.const 1))
(assert_return (invoke "i64.rem_s_7" (i64.const 0x7000000000000000)) (i64.const 0))
(assert_return (invoke "i64.rem_u_7" (i64.const 71)) (i64.const 1))
(assert_return (invoke "i64.rem_u_7" (i64.const 0xe000000000000000)) (i64.const 0))


