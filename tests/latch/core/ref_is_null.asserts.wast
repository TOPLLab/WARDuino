;; https://github.com/WebAssembly/testsuite/blob/6aacfd8929504d8e02a5144a14d184196ede6790/ref_is_null.wast
;; NOTE: these tests were modified in order to work with WARDuino
;; multiple tables and reference argument passing was not supported at the time of writing

(assert_return (invoke "funcref") (i32.const 1))
(assert_return (invoke "externref") (i32.const 1))

;; (assert_return (invoke "init" (ref.extern 0)) (i32.const 0))

(assert_return (invoke "funcref-elem" (i32.const 0)) (i32.const 1))
;; (assert_return (invoke "externref-elem" (i32.const 0)) (i32.const 1))

(assert_return (invoke "funcref-elem" (i32.const 1)) (i32.const 0))
;; (assert_return (invoke "externref-elem" (i32.const 1)) (i32.const 0))

(assert_return (invoke "deinit"))

(assert_return (invoke "funcref-elem" (i32.const 0)) (i32.const 1))
;; (assert_return (invoke "externref-elem" (i32.const 0)) (i32.const 1))

(assert_return (invoke "funcref-elem" (i32.const 1)) (i32.const 1))
;; (assert_return (invoke "externref-elem" (i32.const 1)) (i32.const 1))
