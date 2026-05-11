(assert_return (invoke "grow" (i32.const 0)) (i32.const 0))
(assert_return (invoke "grow" (i32.const 1)) (i32.const 0))
(assert_return (invoke "grow" (i32.const 0)) (i32.const 1))
(assert_return (invoke "grow" (i32.const 2)) (i32.const 1))
(assert_return (invoke "grow" (i32.const 800)) (i32.const 3))
(assert_return (invoke "grow" (i32.const 0x10000)) (i32.const -1))
(assert_return (invoke "grow" (i32.const 64736)) (i32.const -1))
(assert_return (invoke "grow" (i32.const 1)) (i32.const 803))

