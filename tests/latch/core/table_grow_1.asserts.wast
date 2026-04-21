;; https://github.com/WebAssembly/testsuite/blob/7ef86ddeed81458f9031a49a40b3a3f99c1c6a8a/table_grow.wast
(assert_return (invoke "grow") (i32.const -1))