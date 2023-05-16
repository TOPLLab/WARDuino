(module
  (memory 1 1)
  (func (export "test")
    (memory.copy (i32.const 0xFF00) (i32.const 0x8000) (i32.const 257))))
