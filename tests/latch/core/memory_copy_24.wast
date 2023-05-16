(module
 (memory 1 1)
 (func (export "test")
   (memory.copy (i32.const 0x4000) (i32.const 0xFFFFFF00) (i32.const 257))))
