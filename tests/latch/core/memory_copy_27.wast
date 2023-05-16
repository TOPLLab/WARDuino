(module
  (memory 1 1)
  (func (export "test")
    (memory.copy (i32.const 0x20000) (i32.const 0x7000) (i32.const 0))))
