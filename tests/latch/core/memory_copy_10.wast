(module
  (memory (export "mem") 1 1 )
  (data (i32.const 65516) "\00\01\02\03\04\05\06\07\08\09\0a\0b\0c\0d\0e\0f\10\11\12\13")
  (func (export "run") (param $targetOffs i32) (param $srcOffs i32) (param $len i32)
    (memory.copy (local.get $targetOffs) (local.get $srcOffs) (local.get $len)))
  (func (export "load8_u") (param i32) (result i32)
    (i32.load8_u (local.get 0))))

