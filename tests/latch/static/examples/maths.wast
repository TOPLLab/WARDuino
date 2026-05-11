(module
  (type $i32->i32->void (func (param i32 i32)))
  (type $i32->void      (func (param i32)))
  (type $i32->i32       (func (param i32) (result i32)))
  (type $void->void     (func (param) (result)))

  (export "fac"  (func $fac))
  (export "fib"  (func $fib))
  (export "mul"  (func $mul))
  (export "gcd"  (func $gcd))
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
    (func $fib (param i32) (result i32)
    (if (result i32) (i32.le_u (local.get 0) (i32.const 2))
      (then (i32.const 1))
      (else
        (i32.add
          (call $fib (i32.sub (local.get 0) (i32.const 2)))
          (call $fib (i32.sub (local.get 0) (i32.const 1)))
        )
      )
    )
  )

  ;; multiplicative fibonacci
    (func $mul (param i32) (result i32)
    (if (result i32) (i32.le_u (local.get 0) (i32.const 2))
      (then (i32.const 1))
      (else
        (i32.mul
          (call $fib (i32.sub (local.get 0) (i32.const 2)))
          (call $fib (i32.sub (local.get 0) (i32.const 1)))
        )
      )
    )
  )

  ;; gcd
  (func $gcd (param i32 i32) (result i32)
    (local i32)
    block  ;; label = @1
      block  ;; label = @2
        local.get 0
        br_if 0 (;@2;)
        local.get 1
        local.set 2
        br 1 (;@1;)
      end
      loop  ;; label = @2
        local.get 1
        local.get 0
        local.tee 2
        i32.rem_u
        local.set 0
        local.get 2
        local.set 1
        local.get 0
        br_if 0 (;@2;)
      end
    end
    local.get 2
  )

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
