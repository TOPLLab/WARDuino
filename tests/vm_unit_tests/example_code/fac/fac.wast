(module

 (; Type declarations ;)
 (type $i2v (func (param i32) (result)))
 (type $i32toi32 (func (param i32) (result i32)))
 (type $v2v (func (param) (result)))

 (; Define one function ;)
 (export "main" (func $main))

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

 (func $main (type $v2v)
    (local $arg i32)
    (local.set $arg (i32.const 5))
    (loop 
       (local.get $arg)
       (call $fac)
        nop
       (br 0)))
)
