
(module
  ;; Auxiliary definitions
  (func $const-i32 (result i32) (i32.const 0x132))
  (func $const-i64 (result i64) (i64.const 0x164))
  (func $const-f32 (result f32) (f32.const 0xf32))
  (func $const-f64 (result f64) (f64.const 0xf64))
  (func $const-i32-i64 (result i32 i64) (i32.const 0x132) (i64.const 0x164))

  (func $id-i32 (param i32) (result i32) (local.get 0))
  (func $id-i64 (param i64) (result i64) (local.get 0))
  (func $id-f32 (param f32) (result f32) (local.get 0))
  (func $id-f64 (param f64) (result f64) (local.get 0))
  (func $id-i32-f64 (param i32 f64) (result i32 f64)
    (local.get 0) (local.get 1)
  )

  (func $swap-i32-i32 (param i32 i32) (result i32 i32)
    (local.get 1) (local.get 0)
  )
  (func $swap-f32-f64 (param f32 f64) (result f64 f32)
    (local.get 1) (local.get 0)
  )
  (func $swap-f64-i32 (param f64 i32) (result i32 f64)
    (local.get 1) (local.get 0)
  )

  (func $f32-i32 (param f32 i32) (result i32) (local.get 1))
  (func $i32-i64 (param i32 i64) (result i64) (local.get 1))
  (func $f64-f32 (param f64 f32) (result f32) (local.get 1))
  (func $i64-f64 (param i64 f64) (result f64) (local.get 1))

  ;; Typing

  (func (export "type-i32") (result i32) (call $const-i32))
  (func (export "type-i64") (result i64) (call $const-i64))
  (func (export "type-f32") (result f32) (call $const-f32))
  (func (export "type-f64") (result f64) (call $const-f64))
  (func (export "type-i32-i64") (result i32 i64) (call $const-i32-i64))

  (func (export "type-first-i32") (result i32) (call $id-i32 (i32.const 32)))
  (func (export "type-first-i64") (result i64) (call $id-i64 (i64.const 64)))
  (func (export "type-first-f32") (result f32) (call $id-f32 (f32.const 1.32)))
  (func (export "type-first-f64") (result f64) (call $id-f64 (f64.const 1.64)))

  (func (export "type-second-i32") (result i32)
    (call $f32-i32 (f32.const 32.1) (i32.const 32))
  )
  (func (export "type-second-i64") (result i64)
    (call $i32-i64 (i32.const 32) (i64.const 64))
  )
  (func (export "type-second-f32") (result f32)
    (call $f64-f32 (f64.const 64) (f32.const 32))
  )
  (func (export "type-second-f64") (result f64)
    (call $i64-f64 (i64.const 64) (f64.const 64.1))
  )

  (func (export "type-all-i32-f64") (result i32 f64)
    (call $id-i32-f64 (i32.const 32) (f64.const 1.64))
  )
  (func (export "type-all-i32-i32") (result i32 i32)
    (call $swap-i32-i32 (i32.const 1) (i32.const 2))
  )
  (func (export "type-all-f32-f64") (result f64 f32)
    (call $swap-f32-f64 (f32.const 1) (f64.const 2))
  )
  (func (export "type-all-f64-i32") (result i32 f64)
    (call $swap-f64-i32 (f64.const 1) (i32.const 2))
  )

  ;; Composition

  (func (export "as-binary-all-operands") (result i32)
    (i32.add (call $swap-i32-i32 (i32.const 3) (i32.const 4)))
  )

  (func (export "as-mixed-operands") (result i32)
    (call $swap-i32-i32 (i32.const 3) (i32.const 4))
    (i32.const 5)
    (i32.add)
    (i32.mul)
  )

  (func (export "as-call-all-operands") (result i32 i32)
    (call $swap-i32-i32 (call $swap-i32-i32 (i32.const 3) (i32.const 4)))
  )

  ;; Recursion

  (func $fac (export "fac") (param i64) (result i64)
    (if (result i64) (i64.eqz (local.get 0))
      (then (i64.const 1))
      (else
        (i64.mul
          (local.get 0)
          (call $fac (i64.sub (local.get 0) (i64.const 1)))
        )
      )
    )
  )

  (func $fac-acc (export "fac-acc") (param i64 i64) (result i64)
    (if (result i64) (i64.eqz (local.get 0))
      (then (local.get 1))
      (else
        (call $fac-acc
          (i64.sub (local.get 0) (i64.const 1))
          (i64.mul (local.get 0) (local.get 1))
        )
      )
    )
  )

  (func $fib (export "fib") (param i64) (result i64)
    (if (result i64) (i64.le_u (local.get 0) (i64.const 1))
      (then (i64.const 1))
      (else
        (i64.add
          (call $fib (i64.sub (local.get 0) (i64.const 2)))
          (call $fib (i64.sub (local.get 0) (i64.const 1)))
        )
      )
    )
  )

  (func $even (export "even") (param i64) (result i32)
    (if (result i32) (i64.eqz (local.get 0))
      (then (i32.const 44))
      (else (call $odd (i64.sub (local.get 0) (i64.const 1))))
    )
  )
  (func $odd (export "odd") (param i64) (result i32)
    (if (result i32) (i64.eqz (local.get 0))
      (then (i32.const 99))
      (else (call $even (i64.sub (local.get 0) (i64.const 1))))
    )
  )

  ;; Stack exhaustion

  ;; Implementations are required to have every call consume some abstract
  ;; resource towards exhausting some abstract finite limit, such that
  ;; infinitely recursive test cases reliably trap in finite time. This is
  ;; because otherwise applications could come to depend on it on those
  ;; implementations and be incompatible with implementations that don't do
  ;; it (or don't do it under the same circumstances).

  (func $runaway (export "runaway") (call $runaway))

  (func $mutual-runaway1 (export "mutual-runaway") (call $mutual-runaway2))
  (func $mutual-runaway2 (call $mutual-runaway1))

  ;; As parameter of control constructs and instructions

  (memory 1)

  (func (export "as-select-first") (result i32)
    (select (call $const-i32) (i32.const 2) (i32.const 3))
  )
  (func (export "as-select-mid") (result i32)
    (select (i32.const 2) (call $const-i32) (i32.const 3))
  )
  (func (export "as-select-last") (result i32)
    (select (i32.const 2) (i32.const 3) (call $const-i32))
  )

  (func (export "as-if-condition") (result i32)
    (if (result i32) (call $const-i32) (then (i32.const 1)) (else (i32.const 2)))
  )

  (func (export "as-br_if-first") (result i32)
    (block (result i32) (br_if 0 (call $const-i32) (i32.const 2)))
  )
  (func (export "as-br_if-last") (result i32)
    (block (result i32) (br_if 0 (i32.const 2) (call $const-i32)))
  )

  (func (export "as-br_table-first") (result i32)
    (block (result i32) (call $const-i32) (i32.const 2) (br_table 0 0))
  )
  (func (export "as-br_table-last") (result i32)
    (block (result i32) (i32.const 2) (call $const-i32) (br_table 0 0))
  )

  (func $func (param i32 i32) (result i32) (local.get 0))
  (type $check (func (param i32 i32) (result i32)))
  (table funcref (elem $func))
  (func (export "as-call_indirect-first") (result i32)
    (block (result i32)
      (call_indirect (type $check)
        (call $const-i32) (i32.const 2) (i32.const 0)
      )
    )
  )
  (func (export "as-call_indirect-mid") (result i32)
    (block (result i32)
      (call_indirect (type $check)
        (i32.const 2) (call $const-i32) (i32.const 0)
      )
    )
  )
  (func (export "as-call_indirect-last") (result i32)
    (block (result i32)
      (call_indirect (type $check)
        (i32.const 1) (i32.const 2) (call $const-i32)
      )
    )
  )

  (func (export "as-store-first")
    (call $const-i32) (i32.const 1) (i32.store)
  )
  (func (export "as-store-last")
    (i32.const 10) (call $const-i32) (i32.store)
  )

  (func (export "as-memory.grow-value") (result i32)
    (memory.grow (call $const-i32))
  )
  (func (export "as-return-value") (result i32)
    (call $const-i32) (return)
  )
  (func (export "as-drop-operand")
    (call $const-i32) (drop)
  )
  (func (export "as-br-value") (result i32)
    (block (result i32) (br 0 (call $const-i32)))
  )
  (func (export "as-local.set-value") (result i32)
    (local i32) (local.set 0 (call $const-i32)) (local.get 0)
  )
  (func (export "as-local.tee-value") (result i32)
    (local i32) (local.tee 0 (call $const-i32))
  )
  (global $a (mut i32) (i32.const 10))
  (func (export "as-global.set-value") (result i32)
    (global.set $a (call $const-i32))
    (global.get $a)
  )
  (func (export "as-load-operand") (result i32)
    (i32.load (call $const-i32))
  )

  (func $dummy (param i32) (result i32) (local.get 0))
  (func $du (param f32) (result f32) (local.get 0))
  (func (export "as-unary-operand") (result f32)
    (block (result f32) (f32.sqrt (call $du (f32.const 0x0p+0))))
  )

  (func (export "as-binary-left") (result i32)
    (block (result i32) (i32.add (call $dummy (i32.const 1)) (i32.const 10)))
  )
  (func (export "as-binary-right") (result i32)
    (block (result i32) (i32.sub (i32.const 10) (call $dummy (i32.const 1))))
  )

  (func (export "as-test-operand") (result i32)
    (block (result i32) (i32.eqz (call $dummy (i32.const 1))))
  )

  (func (export "as-compare-left") (result i32)
    (block (result i32) (i32.le_u (call $dummy (i32.const 1)) (i32.const 10)))
  )
  (func (export "as-compare-right") (result i32)
    (block (result i32) (i32.ne (i32.const 10) (call $dummy (i32.const 1))))
  )

  (func (export "as-convert-operand") (result i64)
    (block (result i64) (i64.extend_i32_s (call $dummy (i32.const 1))))
  )

  ;; Test correct argument passing

  (func $return-from-long-argument-list-helper (param f32 i32 i32 f64 f32 f32 f32 f64 f32 i32 i32 f32 f64 i64 i64 i32 i64 i64 f32 i64 i64 i64 i32 f32 f32 f32 f64 f32 i32 i64 f32 f64 f64 f32 i32 f32 f32 f64 i64 f64 i32 i64 f32 f64 i32 i32 i32 i64 f64 i32 i64 i64 f64 f64 f64 f64 f64 f64 i32 f32 f64 f64 i32 i64 f32 f32 f32 i32 f64 f64 f64 f64 f64 f32 i64 i64 i32 i32 i32 f32 f64 i32 i64 f32 f32 f32 i32 i32 f32 f64 i64 f32 f64 f32 f32 f32 i32 f32 i64 i32) (result i32)
    (local.get 99)
  )

  (func (export "return-from-long-argument-list") (param i32) (result i32)
    (call $return-from-long-argument-list-helper (f32.const 0) (i32.const 0) (i32.const 0) (f64.const 0) (f32.const 0) (f32.const 0) (f32.const 0) (f64.const 0) (f32.const 0) (i32.const 0) (i32.const 0) (f32.const 0) (f64.const 0) (i64.const 0) (i64.const 0) (i32.const 0) (i64.const 0) (i64.const 0) (f32.const 0) (i64.const 0) (i64.const 0) (i64.const 0) (i32.const 0) (f32.const 0) (f32.const 0) (f32.const 0) (f64.const 0) (f32.const 0) (i32.const 0) (i64.const 0) (f32.const 0) (f64.const 0) (f64.const 0) (f32.const 0) (i32.const 0) (f32.const 0) (f32.const 0) (f64.const 0) (i64.const 0) (f64.const 0) (i32.const 0) (i64.const 0) (f32.const 0) (f64.const 0) (i32.const 0) (i32.const 0) (i32.const 0) (i64.const 0) (f64.const 0) (i32.const 0) (i64.const 0) (i64.const 0) (f64.const 0) (f64.const 0) (f64.const 0) (f64.const 0) (f64.const 0) (f64.const 0) (i32.const 0) (f32.const 0) (f64.const 0) (f64.const 0) (i32.const 0) (i64.const 0) (f32.const 0) (f32.const 0) (f32.const 0) (i32.const 0) (f64.const 0) (f64.const 0) (f64.const 0) (f64.const 0) (f64.const 0) (f32.const 0) (i64.const 0) (i64.const 0) (i32.const 0) (i32.const 0) (i32.const 0) (f32.const 0) (f64.const 0) (i32.const 0) (i64.const 0) (f32.const 0) (f32.const 0) (f32.const 0) (i32.const 0) (i32.const 0) (f32.const 0) (f64.const 0) (i64.const 0) (f32.const 0) (f64.const 0) (f32.const 0) (f32.const 0) (f32.const 0) (i32.const 0) (f32.const 0) (i64.const 0) (local.get 0))
  )
)

