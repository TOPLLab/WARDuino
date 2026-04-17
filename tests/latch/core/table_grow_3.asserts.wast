;; https://github.com/WebAssembly/testsuite/blob/7ef86ddeed81458f9031a49a40b3a3f99c1c6a8a/table_grow.wast
(assert_return (invoke "grow" (i32.const 0)) (i32.const 0))
(assert_return (invoke "grow" (i32.const 1)) (i32.const 0))
(assert_return (invoke "grow" (i32.const 1)) (i32.const 1))
(assert_return (invoke "grow" (i32.const 2)) (i32.const 2))
(assert_return (invoke "grow" (i32.const 6)) (i32.const 4))
(assert_return (invoke "grow" (i32.const 0)) (i32.const 10))
(assert_return (invoke "grow" (i32.const 1)) (i32.const -1))
(assert_return (invoke "grow" (i32.const 0x10000)) (i32.const -1))
