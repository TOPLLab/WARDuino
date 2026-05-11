(module
  ;; Test that we can use indices instead of names to reference imports,
  ;; exports, functions and parameters.
  (import "spectest" "print_i32" (func (param i32)))
  (func (import "spectest" "print_i32") (param i32))
  (func (param i32) (param i32)
    (call 0 (local.get 0))
    (call 1 (local.get 1))
  )
  (export "print32" (func 2))
)

