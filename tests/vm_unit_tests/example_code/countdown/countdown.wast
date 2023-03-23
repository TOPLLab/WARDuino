(module

 (; Type declarations ;)
 (type $i2v (func (param i64) (result)))
 (type $i2i (func (param i64) (result i64)))
 (type $v2v (func (param) (result)))

 (; Define one function ;)
 (export "main" (func $main))
 (memory 1)
 (table funcref (elem $countdown $start))

 (global $g1  i32   (i32.const 0))
 (global $g2 (mut i32) (i32.const 0))

 (func $start (type $i2v))
 (func $countdown (type $i2i)
     (i64.gt_s
       (local.get 0)
       (i64.const 0))
     (if (result i64)
       (then
         (i64.sub
           (local.get 0)
           (i64.const 1))
         (call $countdown))
       (else
          (i64.const 0))))

 (func $main (type $v2v)
    (loop
       (i64.const 5)
       (call $countdown)
       (call $start)
       (br 0)))
)