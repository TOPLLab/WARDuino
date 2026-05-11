;; https://github.com/WebAssembly/testsuite/blob/1b45c074228bc47f5f1c718a0929e7a3a2c5bb21/select.wast

(assert_return (invoke "select_i32" (i32.const 1) (i32.const 2) (i32.const 1)) (i32.const 1))
(assert_return (invoke "select_i64" (i64.const 2) (i64.const 1) (i32.const 1)) (i64.const 2))
(assert_return (invoke "select_f32" (f32.const 1) (f32.const 2) (i32.const 1)) (f32.const 1))
(assert_return (invoke "select_f64" (f64.const 1) (f64.const 2) (i32.const 1)) (f64.const 1))

(assert_return (invoke "select_i32" (i32.const 1) (i32.const 2) (i32.const 0)) (i32.const 2))
(assert_return (invoke "select_i32" (i32.const 2) (i32.const 1) (i32.const 0)) (i32.const 1))
(assert_return (invoke "select_i64" (i64.const 2) (i64.const 1) (i32.const -1)) (i64.const 2))
(assert_return (invoke "select_i64" (i64.const 2) (i64.const 1) (i32.const 0xf0f0f0f0)) (i64.const 2))

(assert_return (invoke "select_f32" (f32.const nan) (f32.const 1) (i32.const 1)) (f32.const nan))
(assert_return (invoke "select_f32" (f32.const nan:0x20304) (f32.const 1) (i32.const 1)) (f32.const nan:0x20304))
(assert_return (invoke "select_f32" (f32.const nan) (f32.const 1) (i32.const 0)) (f32.const 1))
(assert_return (invoke "select_f32" (f32.const nan:0x20304) (f32.const 1) (i32.const 0)) (f32.const 1))
(assert_return (invoke "select_f32" (f32.const 2) (f32.const nan) (i32.const 1)) (f32.const 2))
(assert_return (invoke "select_f32" (f32.const 2) (f32.const nan:0x20304) (i32.const 1)) (f32.const 2))
(assert_return (invoke "select_f32" (f32.const 2) (f32.const nan) (i32.const 0)) (f32.const nan))
(assert_return (invoke "select_f32" (f32.const 2) (f32.const nan:0x20304) (i32.const 0)) (f32.const nan:0x20304))

(assert_return (invoke "select_f64" (f64.const nan) (f64.const 1) (i32.const 1)) (f64.const nan))
(assert_return (invoke "select_f64" (f64.const nan:0x20304) (f64.const 1) (i32.const 1)) (f64.const nan:0x20304))
(assert_return (invoke "select_f64" (f64.const nan) (f64.const 1) (i32.const 0)) (f64.const 1))
(assert_return (invoke "select_f64" (f64.const nan:0x20304) (f64.const 1) (i32.const 0)) (f64.const 1))
(assert_return (invoke "select_f64" (f64.const 2) (f64.const nan) (i32.const 1)) (f64.const 2))
(assert_return (invoke "select_f64" (f64.const 2) (f64.const nan:0x20304) (i32.const 1)) (f64.const 2))
(assert_return (invoke "select_f64" (f64.const 2) (f64.const nan) (i32.const 0)) (f64.const nan))
(assert_return (invoke "select_f64" (f64.const 2) (f64.const nan:0x20304) (i32.const 0)) (f64.const nan:0x20304))

(assert_trap (invoke "select_trap_l" (i32.const 1)) "unreachable")
(assert_trap (invoke "select_trap_l" (i32.const 0)) "unreachable")
(assert_trap (invoke "select_trap_r" (i32.const 1)) "unreachable")
(assert_trap (invoke "select_trap_r" (i32.const 0)) "unreachable")

(assert_return (invoke "as-select-first" (i32.const 0)) (i32.const 1))
(assert_return (invoke "as-select-first" (i32.const 1)) (i32.const 0))
(assert_return (invoke "as-select-mid" (i32.const 0)) (i32.const 2))
(assert_return (invoke "as-select-mid" (i32.const 1)) (i32.const 2))
(assert_return (invoke "as-select-last" (i32.const 0)) (i32.const 2))
(assert_return (invoke "as-select-last" (i32.const 1)) (i32.const 3))

(assert_return (invoke "as-loop-first" (i32.const 0)) (i32.const 3))
(assert_return (invoke "as-loop-first" (i32.const 1)) (i32.const 2))
(assert_return (invoke "as-loop-mid" (i32.const 0)) (i32.const 3))
(assert_return (invoke "as-loop-mid" (i32.const 1)) (i32.const 2))
(assert_return (invoke "as-loop-last" (i32.const 0)) (i32.const 3))
(assert_return (invoke "as-loop-last" (i32.const 1)) (i32.const 2))

(assert_return (invoke "as-if-condition" (i32.const 0)))
(assert_return (invoke "as-if-condition" (i32.const 1)))
(assert_return (invoke "as-if-then" (i32.const 0)) (i32.const 3))
(assert_return (invoke "as-if-then" (i32.const 1)) (i32.const 2))
(assert_return (invoke "as-if-else" (i32.const 0)) (i32.const 3))
(assert_return (invoke "as-if-else" (i32.const 1)) (i32.const 2))

(assert_return (invoke "as-br_if-first" (i32.const 0)) (i32.const 3))
(assert_return (invoke "as-br_if-first" (i32.const 1)) (i32.const 2))
(assert_return (invoke "as-br_if-last" (i32.const 0)) (i32.const 2))
(assert_return (invoke "as-br_if-last" (i32.const 1)) (i32.const 2))

(assert_return (invoke "as-br_table-first" (i32.const 0)) (i32.const 3))
(assert_return (invoke "as-br_table-first" (i32.const 1)) (i32.const 2))
(assert_return (invoke "as-br_table-last" (i32.const 0)) (i32.const 2))
(assert_return (invoke "as-br_table-last" (i32.const 1)) (i32.const 2))

(assert_return (invoke "as-call_indirect-first" (i32.const 0)) (i32.const 3))
(assert_return (invoke "as-call_indirect-first" (i32.const 1)) (i32.const 2))
(assert_return (invoke "as-call_indirect-mid" (i32.const 0)) (i32.const 1))
(assert_return (invoke "as-call_indirect-mid" (i32.const 1)) (i32.const 1))
(assert_trap (invoke "as-call_indirect-last" (i32.const 0)) "undefined element")
(assert_trap (invoke "as-call_indirect-last" (i32.const 1)) "undefined element")

(assert_return (invoke "as-store-first" (i32.const 0)))
(assert_return (invoke "as-store-first" (i32.const 1)))
(assert_return (invoke "as-store-last" (i32.const 0)))
(assert_return (invoke "as-store-last" (i32.const 1)))

(assert_return (invoke "as-memory.grow-value" (i32.const 0)) (i32.const 1))
(assert_return (invoke "as-memory.grow-value" (i32.const 1)) (i32.const 3))

(assert_return (invoke "as-call-value" (i32.const 0)) (i32.const 2))
(assert_return (invoke "as-call-value" (i32.const 1)) (i32.const 1))
(assert_return (invoke "as-return-value" (i32.const 0)) (i32.const 2))
(assert_return (invoke "as-return-value" (i32.const 1)) (i32.const 1))
(assert_return (invoke "as-drop-operand" (i32.const 0)))
(assert_return (invoke "as-drop-operand" (i32.const 1)))
(assert_return (invoke "as-br-value" (i32.const 0)) (i32.const 2))
(assert_return (invoke "as-br-value" (i32.const 1)) (i32.const 1))
(assert_return (invoke "as-local.set-value" (i32.const 0)) (i32.const 2))
(assert_return (invoke "as-local.set-value" (i32.const 1)) (i32.const 1))
(assert_return (invoke "as-local.tee-value" (i32.const 0)) (i32.const 2))
(assert_return (invoke "as-local.tee-value" (i32.const 1)) (i32.const 1))
(assert_return (invoke "as-global.set-value" (i32.const 0)) (i32.const 2))
(assert_return (invoke "as-global.set-value" (i32.const 1)) (i32.const 1))
(assert_return (invoke "as-load-operand" (i32.const 0)) (i32.const 1))
(assert_return (invoke "as-load-operand" (i32.const 1)) (i32.const 1))

(assert_return (invoke "as-unary-operand" (i32.const 0)) (i32.const 0))
(assert_return (invoke "as-unary-operand" (i32.const 1)) (i32.const 1))
(assert_return (invoke "as-binary-operand" (i32.const 0)) (i32.const 4))
(assert_return (invoke "as-binary-operand" (i32.const 1)) (i32.const 1))
(assert_return (invoke "as-test-operand" (i32.const 0)) (i32.const 0))
(assert_return (invoke "as-test-operand" (i32.const 1)) (i32.const 1))
(assert_return (invoke "as-compare-left" (i32.const 0)) (i32.const 0))
(assert_return (invoke "as-compare-left" (i32.const 1)) (i32.const 1))
(assert_return (invoke "as-compare-right" (i32.const 0)) (i32.const 0))
(assert_return (invoke "as-compare-right" (i32.const 1)) (i32.const 1))
(assert_return (invoke "as-convert-operand" (i32.const 0)) (i32.const 0))
(assert_return (invoke "as-convert-operand" (i32.const 1)) (i32.const 1))

(assert_invalid
  (module (func $arity-0 (select (nop) (nop) (i32.const 1))))
  "type mismatch"
)

;; The first two operands should have the same type as each other

(assert_invalid
  (module (func $type-num-vs-num (select (i32.const 1) (i64.const 1) (i32.const 1))))
  "type mismatch"
)
(assert_invalid
  (module (func $type-num-vs-num (select (i32.const 1) (f32.const 1.0) (i32.const 1))))
  "type mismatch"
)
(assert_invalid
  (module (func $type-num-vs-num (select (i32.const 1) (f64.const 1.0) (i32.const 1))))
  "type mismatch"
)


(assert_invalid
  (module
    (func $type-1st-operand-empty
      (select) (drop)
    )
  )
  "type mismatch"
)
(assert_invalid
  (module
    (func $type-2nd-operand-empty
      (i32.const 0) (select) (drop)
    )
  )
  "type mismatch"
)
(assert_invalid
  (module
    (func $type-3rd-operand-empty
      (i32.const 0) (i32.const 0) (select) (drop)
    )
  )
  "type mismatch"
)
(assert_invalid
  (module
    (func $type-1st-operand-empty-in-block
      (i32.const 0) (i32.const 0) (i32.const 0)
      (block (select) (drop))
    )
  )
  "type mismatch"
)
(assert_invalid
  (module
    (func $type-2nd-operand-empty-in-block
      (i32.const 0) (i32.const 0)
      (block (i32.const 0) (select) (drop))
    )
  )
  "type mismatch"
)
(assert_invalid
  (module
    (func $type-3rd-operand-empty-in-block
      (i32.const 0)
      (block (i32.const 0) (i32.const 0) (select) (drop))
    )
  )
  "type mismatch"
)
(assert_invalid
  (module
    (func $type-1st-operand-empty-in-loop
      (i32.const 0) (i32.const 0) (i32.const 0)
      (loop (select) (drop))
    )
  )
  "type mismatch"
)
(assert_invalid
  (module
    (func $type-2nd-operand-empty-in-loop
      (i32.const 0) (i32.const 0)
      (loop (i32.const 0) (select) (drop))
    )
  )
  "type mismatch"
)
(assert_invalid
  (module
    (func $type-3rd-operand-empty-in-loop
      (i32.const 0)
      (loop (i32.const 0) (i32.const 0) (select) (drop))
    )
  )
  "type mismatch"
)
(assert_invalid
  (module
    (func $type-1st-operand-empty-in-then
      (i32.const 0) (i32.const 0) (i32.const 0)
      (if (then (select) (drop)))
    )
  )
  "type mismatch"
)
(assert_invalid
  (module
    (func $type-2nd-operand-empty-in-then
      (i32.const 0) (i32.const 0)
      (if (then (i32.const 0) (select) (drop)))
    )
  )
  "type mismatch"
)
(assert_invalid
  (module
    (func $type-3rd-operand-empty-in-then
      (i32.const 0)
      (if (then (i32.const 0) (i32.const 0) (select) (drop)))
    )
  )
  "type mismatch"
)