(module

 (; Type declarations ;)
 (type $i2v (func (param i32) (result)))
 (type $v2v (func (param) (result)))
 (type $i2i (func (param i64) (param i32) (result i32)))

 (; Define one function ;)
 (export "main" (func $fac5))

 (memory 1)
 (table $funcs 3 anyfunc)
 (global $i32 (mut i32) (i32.const 0))
 (global $ai32 (mut i32) (i32.const 0))

 (global $f64 (mut f64) (f64.const 0))
 (global $af64 (mut f64) (f64.const 0))


 (func $dummy (type $i2v))
 (func $fac (type $i2i)
     (i32.gt_s
       (local.get 1)
       (i32.const 1)
     )
     (if (result i32)
       (then 

         (i64.add
           (local.get 0)
           (i64.const 1)
          )
         (i32.sub 
           (local.get 1)
           (i32.const 1))

         (call $fac)

         (local.get 1)
         i32.mul
       )
       (else
         (global.set $i32 (i32.const 32))
         (global.set $f64 (f64.const 64))

         (global.set $ai32
                  (i32.add (global.get $i32)
                           (global.get $i32)))
				 (global.set $af64
									(f64.add (global.get $f64)
													 (global.get $f64)))
				 (i32.const 1)
         )
     )
 )

   (;main func loops forever;)
 (func $fac5 (type $v2v)
       (local $foo i32)
       (local.set $foo (i32.const 6))
    (loop 

      (i64.const 13)
      (get_local $foo)
      (call $fac)

      (call $dummy) 
      (br 0)
      )
  )
 (elem (i32.const 0) $fac5 $fac $dummy)
)

