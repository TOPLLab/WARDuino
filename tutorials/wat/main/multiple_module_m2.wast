(module
  (import "env" "print_string" (func $print (param i32 i32)))
  (import "multiple_module_m1" "add_ten" (func $add_ten (param i32) (result i32)))
  
  (func $add_twenty (export "add_twenty") (param $x i32) (result i32)
    (local.get $x)
    (call $add_ten)
    (call $add_ten)
  )
)