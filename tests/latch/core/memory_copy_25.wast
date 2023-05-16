(module
  (memory 1 1)
  (func (export "test")
    (memory.fill (i32.const 0x0000) (i32.const 0x55) (i32.const 0x8000))
    (memory.fill (i32.const 0x8000) (i32.const 0xAA) (i32.const 0x8000))
    (memory.copy (i32.const 0x9000) (i32.const 0x7000) (i32.const 0)))
  
  (func (export "checkRange") (param $from i32) (param $to i32) (param $expected i32) (result i32)
    (loop $cont
      (if (i32.eq (local.get $from) (local.get $to))
        (then
          (return (i32.const -1))))
      (if (i32.eq (i32.load8_u (local.get $from)) (local.get $expected))
        (then
          (local.set $from (i32.add (local.get $from) (i32.const 1)))
          (br $cont))))
    (return (local.get $from)))
)
