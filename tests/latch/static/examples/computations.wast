(module
  (type $i32->i32->void (func (param i32 i32)))
  (type $i32->void      (func (param i32)))
  (type $i32->i32       (func (param i32) (result i32)))
  (type $void->void     (func (param) (result)))

  (export "fac"  (func $fac))
  (export "fib"  (func $fac))
  (export "prim" (func $prim))
 
  ;; factorial
  (func $fac (type $i32->i32)
    (i32.gt_s
      (local.get 0)
      (i32.const 1))
    (if (result i32)
      (then 
        (i32.sub 
          (local.get 0)
          (i32.const 1))
        (call $fac)
        (local.get 0)
        i32.mul)
      (else
        (i32.const 1))))

  ;; fibonacci
    (func $fib (param i64) (result i64)
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
  
;;  (func $fib (param i32) (result i32)
;;    (if (result i32) (i32.eqz (local.get 0))
;;      (then (i32.const 0))
;;      (else
;;        (if (result i32) (i32.le_u (local.get 0) (i32.const 2))
;;          (then (i32.const 1))
;;          (else
;;            (i32.add
;;              (call $fib (i32.sub (local.get 0) (i32.const 2)))
;;              (call $fib (i32.sub (local.get 0) (i32.const 1)))
;;            )
;;           )
;;         )
;;      )
;;    )
;;  )

    ;; primes
  (func $prim (param i32) (result i32)
    (local i32)
    i32.const 1
    local.set 1
    block  ;; label = @1
      local.get 0
      i32.const 3
      i32.lt_s
      br_if 0 (;@1;)
      i32.const 2
      local.set 1
      block  ;; label = @2
        loop  ;; label = @3
          local.get 0
          local.get 1
          i32.rem_s
          i32.eqz
          br_if 1 (;@2;)
          local.get 1
          i32.const 1
          i32.add
          local.tee 1
          local.get 0
          i32.lt_s
          br_if 0 (;@3;)
        end
        i32.const 1
        return
      end
      i32.const 0
      local.set 1
    end
    local.get 1)
)
