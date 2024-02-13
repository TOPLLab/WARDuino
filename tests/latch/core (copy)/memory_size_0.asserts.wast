(assert_return (invoke "size") (i32.const 0))
(assert_return (invoke "grow" (i32.const 1)))
(assert_return (invoke "size") (i32.const 1))
(assert_return (invoke "grow" (i32.const 4)))
(assert_return (invoke "size") (i32.const 5))
(assert_return (invoke "grow" (i32.const 0)))
(assert_return (invoke "size") (i32.const 5))

