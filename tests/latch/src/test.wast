(module
  (func (export "get-5-i32") (result i32) (i32.const 5))
  (func (export "get-5-i64") (result i64) (i64.const 5))
  (func (export "get-5-f32") (result f32) (f32.const 5))
  (func (export "get-5-f64") (result f64) (f64.const 5))

  (func (export "check-eq-5-i32") (param i32) (result i32) (i32.eq (local.get 0) (i32.const 5)))
  (func (export "check-eq-5-i64") (param i64) (result i32) (i64.eq (local.get 0) (i64.const 5)))
  (func (export "check-eq-5-f32") (param f32) (result i32) (f32.eq (local.get 0) (f32.const 5)))
  (func (export "check-eq-5-f64") (param f64) (result i32) (f64.eq (local.get 0) (f64.const 5)))

  ;; -- multivalue tests --

  (func (export "get-multi-5-6") (result i32 i32) 
    (i32.const 5)
    (i32.const 6)
  )

  (func (export "check-eq-multi-5-6") (param i32 i32) (result i32)
    (i32.and
      (i32.eq (local.get 0) (i32.const 5))
      (i32.eq (local.get 1) (i32.const 6))
    )
  )

  ;; with blocks
  (func (export "get-multi-block-5-6") (result i32 i32) 
    (block (result i32 i32)
      (i32.const 5)
      (i32.const 6)
    )
  )

  ;; with branching
  (func (export "get-multi-branch-5-6") (result i32 i32) 
    (block (result i32 i32)
      (br 0
        (i32.const 5)
        (i32.const 6)
      )
    )
  )

  ;; with conditional
  (func (export "get-multi-if-5-6") (result i32 i32) 
    (if (result i32 i32)
      (i32.const 1)
      (then
        (i32.const 5)
        (i32.const 6)
      )
      (else
        (i32.const 0)
        (i32.const 0)
      )
    )
  )
)