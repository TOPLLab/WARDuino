(module

 (; Type declarations ;)
 (type $i2v (func (param i32) (result)))
 (type $i32toi32 (func (param i32) (result i32)))
 (type $v2v (func (param) (result)))

 (; Define one function ;)
 (export "main" (func $fac5))

 (func $fac (type $i32toi32)
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

 (func $fac5 (type $v2v)
    (i32.const 5)
    (call $fac)
     drop)
)
