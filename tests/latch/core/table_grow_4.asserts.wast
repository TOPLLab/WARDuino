;; https://github.com/WebAssembly/testsuite/blob/7ef86ddeed81458f9031a49a40b3a3f99c1c6a8a/table_grow.wast

;; TODO: spec test parsing & latch must be modified such that reference types can be used in the tests
;; -> these are commented out for now

;; (assert_return (invoke "check-table-null" (i32.const 0) (i32.const 9)) (ref.null func))
(assert_return (invoke "grow" (i32.const 10)) (i32.const 10))
;; (assert_return (invoke "check-table-null" (i32.const 0) (i32.const 19)) (ref.null func))
