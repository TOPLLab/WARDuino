
(module
  ;; Type declarations
  (type $int32->int32->int32 (func (param i32 i32) (result i32)))
  (type $int32->int32 (func (param i32) (result i32)))
  (type $int32->void (func (param i32)))
  (type $void->void (func))

  ;; Imports from the WARDuino VM
  (import "env" "dummy" (func $env.dummy (type $int32->int32->int32)))
  (import "env" "print_int" (func $env.print_int (type $int32->void)))

  ;; Memory
  (memory 1)

  ;; Main function
  (func $main (export "main") (type $void->void)
    (call $env.dummy (i32.const 32) (i32.const 64))
    (drop)
    (call $env.print_int (i32.load (i32.const 32)))
  )

  (func $dummy (export "dummy") (type $int32->int32->int32)
        (call $env.dummy (local.get 0) (local.get 1)))

  (func $load (export "load") (type $int32->int32)
        (i32.load (local.get 0)))
)
