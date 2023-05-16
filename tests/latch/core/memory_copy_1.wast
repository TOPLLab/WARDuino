(module
  (memory (export "memory0") 1 1)
  (data (i32.const 2) "\03\01\04\01")
  (data (i32.const 12) "\07\05\02\03\06")
  (func (export "test")
    (memory.copy (i32.const 13) (i32.const 2) (i32.const 3)))
  (func (export "load8_u") (param i32) (result i32)
    (i32.load8_u (local.get 0))))

