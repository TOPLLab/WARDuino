;; https://github.com/WebAssembly/testsuite/blob/9828546c5a7b57d40c178bddcc4633b3a11c239f/ref_func.wast
(assert_return (invoke "is_null-f") (i32.const 0))
(assert_return (invoke "is_null-g") (i32.const 0))
(assert_return (invoke "is_null-v") (i32.const 0))

(assert_return (invoke "call-f" (i32.const 4)) (i32.const 4))
(assert_return (invoke "call-g" (i32.const 4)) (i32.const 5))
(assert_return (invoke "call-v" (i32.const 4)) (i32.const 4))
(assert_return (invoke "set-g") ())
(assert_return (invoke "call-v" (i32.const 4)) (i32.const 5))
(assert_return (invoke "set-f") ())
(assert_return (invoke "call-v" (i32.const 4)) (i32.const 4))
