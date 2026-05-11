(module

  (; Imports from the WARDuino VM ;)
  (import "env" "print_string" (func $print.string (type $i32->i32->void)))
  (import "env" "print_int"    (func $print.int    (type $i32->void)))

  (; Type declarations ;)
  (type $i32->i32->void (func (param i32 i32)))
  (type $i32->void      (func (param i32)))
  (type $i32->i32       (func (param i32) (result i32)))
  (type $void->void     (func (param) (result)))

  (; Export two functions ;)
  (export "main" (func $main))
  (export "fac"  (func $fac))
 
  (memory $mem 1)
  (data (i32.const 0) "\nCalculating fac(20) ... ")
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

  (func $main (type $void->void)
    (local $arg i32)
    (local.set $arg (i32.const 30))
    (call $print.string (i32.const 0) (i32.const 25))
    (local.get $arg)
    (call $fac)
    (call $print.int)
    (br 0))
)
