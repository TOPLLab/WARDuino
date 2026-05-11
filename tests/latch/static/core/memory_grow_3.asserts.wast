(assert_return (invoke "check-memory-zero" (i32.const 0) (i32.const 0xffff)) (i32.const 0))
(assert_return (invoke "grow" (i32.const 1)) (i32.const 1))
(assert_return (invoke "check-memory-zero" (i32.const 0x10000) (i32.const 0x1_ffff)) (i32.const 0))
(assert_return (invoke "grow" (i32.const 1)) (i32.const 2))
(assert_return (invoke "check-memory-zero" (i32.const 0x20000) (i32.const 0x2_ffff)) (i32.const 0))
(assert_return (invoke "grow" (i32.const 1)) (i32.const 3))
(assert_return (invoke "check-memory-zero" (i32.const 0x30000) (i32.const 0x3_ffff)) (i32.const 0))
(assert_return (invoke "grow" (i32.const 1)) (i32.const 4))
(assert_return (invoke "check-memory-zero" (i32.const 0x40000) (i32.const 0x4_ffff)) (i32.const 0))
(assert_return (invoke "grow" (i32.const 1)) (i32.const 5))
(assert_return (invoke "check-memory-zero" (i32.const 0x50000) (i32.const 0x5_ffff)) (i32.const 0))


