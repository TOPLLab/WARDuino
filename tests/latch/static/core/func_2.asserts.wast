(assert_return (invoke "f" (i32.const 42)) (i32.const 0))
(assert_return (invoke "g" (i32.const 42)) (i32.const 0))
(assert_return (invoke "p") (i32.const 42))


