(module
  (import "env" "print_string" (func $print (param i32 i32)))
  (table $tbl 5 funcref)
  (memory $mem 1)
  (global $counter (mut i32) (i32.const 100))
  (export "table" (table $tbl))
  (export "shared_mem" (memory $mem))
  (export "get_counter" (func $get_counter))
  (export "inc_counter" (func $inc_counter))
  (export "set_in_memory" (func $set_in_memory))
  
  (data (i32.const 10) "M3: callback executed\n")
  
  (func $callback
    (call $print (i32.const 10) (i32.const 22))
  )
  
  (func $get_counter (result i32)
    (global.get $counter)
  )
  
  (func $inc_counter
    (global.get $counter)
    (i32.const 1)
    (i32.add)
    (global.set $counter)
  )

  (func $set_in_memory (param i32)
    (i32.const 0)
    (local.get 0)
    (i32.store)
  )
  
  (elem (i32.const 0) func $callback)
)