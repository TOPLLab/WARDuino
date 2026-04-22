(module
  (import "env" "print_string" (func $print.string (param i32 i32)))
  (import "env" "print_int"    (func $print.int    (param i32)))

  (memory $mem 1)
  (export "memory" (memory $mem))

  (data (i32.const 0) "Test 1: Basic swap\n")
  (data (i32.const 20) "Test 2: Triple return\n")
  (data (i32.const 43) "Test 3: Nested calls\n")
  (data (i32.const 65) "Test 4: Block multi-value\n")
  (data (i32.const 92) "Test 5: Loop multi-value\n")
  (data (i32.const 118) "Test 6: If multi-value\n")
  (data (i32.const 142) "Test 7: Branch with values\n")
  (data (i32.const 170) "Result: ")
  (data (i32.const 179) "Test 8: br_table multi-value\n")
  (data (i32.const 208) "Test 9: Complex composition\n")

  ;; Test 1: two return values (swap)
  (func $swap (param i32 i32) (result i32 i32)
    (local.get 1) 
    (local.get 0)
  )

  (func $test_swap (export "test_swap")
    (call $print.string (i32.const 0) (i32.const 19))
    
    (i32.const 5)
    (i32.const 10)
    (call $swap)
    
    (call $print.string (i32.const 170) (i32.const 8))
    (call $print.int)
    
    (call $print.string (i32.const 170) (i32.const 8))
    (call $print.int)
  )

  ;; Test 2: three return values
  (func $triple (param i32) (result i32 i32 i32)
    ;; returns (x, x+10, x+20)
    (local.get 0)
    (local.get 0)
    (i32.const 10)
    (i32.add)
    (local.get 0)
    (i32.const 20)
    (i32.add)
  )

  (func $test_triple (export "test_triple")
    (call $print.string (i32.const 20) (i32.const 22))
    
    (i32.const 5)
    (call $triple)
    
    (call $print.string (i32.const 170) (i32.const 8))
    (call $print.int)
    (call $print.string (i32.const 170) (i32.const 8))
    (call $print.int)
    (call $print.string (i32.const 170) (i32.const 8))
    (call $print.int)
  )

  ;; Test 3: nested multi-value calls
  (func $add_pair (param i32 i32) (result i32)
    (local.get 0)
    (local.get 1)
    (i32.add)
  )

  (func $test_nested (export "test_nested")
    (call $print.string (i32.const 43) (i32.const 21))
    
    (i32.const 3)
    (i32.const 7)
    (call $swap)
    (call $add_pair)
    
    (call $print.string (i32.const 170) (i32.const 8))
    (call $print.int)
  )

  ;; Test 4: block with multi-value type  
  (func $test_block (export "test_block")
    (call $print.string (i32.const 65) (i32.const 26))
    
    (block (result i32 i32)
      (i32.const 42)
      (i32.const 99)
    )
    
    (call $print.string (i32.const 170) (i32.const 8))
    (call $print.int)
    (call $print.string (i32.const 170) (i32.const 8))
    (call $print.int)
  )

  ;; Test 5: loop with multi-value
  (func $test_loop (export "test_loop")
    (local $counter i32)
    (local $val1 i32)
    (local $val2 i32)
    (call $print.string (i32.const 92) (i32.const 25))
    
    (i32.const 0)
    (local.set $counter)
    
    (i32.const 10)
    (local.set $val1)
    (i32.const 20)
    (local.set $val2)
    
    ;; loop that accumulates
    (block $exit
      (loop $continue

        (local.get $counter)
        (i32.const 3)
        (i32.ge_u)
        (br_if $exit)
        
        ;; incr both values
        (local.get $val1)
        (i32.const 1)
        (i32.add)
        (local.set $val1)
        
        (local.get $val2)
        (i32.const 2)
        (i32.add)
        (local.set $val2)
        
        ;; incr counter
        (local.get $counter)
        (i32.const 1)
        (i32.add)
        (local.set $counter)
        
        (br $continue)
      )
    )
    
    ;; get results from loop
    (block (result i32 i32)
      (local.get $val1)
      (local.get $val2)
    )
    
    (call $print.string (i32.const 170) (i32.const 8))
    (call $print.int)
    (call $print.string (i32.const 170) (i32.const 8))
    (call $print.int)
  )

  ;; Test 6: if/else with multi-value result
  (func $test_if (export "test_if") (param i32)
    (call $print.string (i32.const 118) (i32.const 23))
    
    (local.get 0)
    (if (result i32 i32)
      (then
        (i32.const 100)
        (i32.const 200)
      )
      (else
        (i32.const 300)
        (i32.const 400)
      )
    )
    
    (call $print.string (i32.const 170) (i32.const 8))
    (call $print.int)
    (call $print.string (i32.const 170) (i32.const 8))
    (call $print.int)
  )

  ;; Test 7: branch with multi-value preversation
  (func $test_branch (export "test_branch") (param i32)
    (call $print.string (i32.const 142) (i32.const 27))
    
    (block (result i32 i32)
      (i32.const 10)
      (i32.const 20)
      
      (local.get 0)
      (br_if 0) 
      
      ;; if we don't branch, we replace the values
      (drop)
      (drop)
      (i32.const 30)
      (i32.const 40)
    )
    
    (call $print.string (i32.const 170) (i32.const 8))
    (call $print.int)
    (call $print.string (i32.const 170) (i32.const 8))
    (call $print.int)
  )

  ;; Test 8: br_table with multi-value
  (func $test_br_table (export "test_br_table") (param i32)
    (local $result1 i32)
    (local $result2 i32)
    (call $print.string (i32.const 179) (i32.const 29))
    
    (block (result i32 i32)
      (block (result i32 i32)
        (block (result i32 i32)
          (i32.const 1)
          (i32.const 2)
          
          (local.get 0)
          (br_table 0 1 2 2) ;; this will branch to case 0, 1, or 2 based on input (with default to 2)
        )
        ;; case 0
        (i32.const 10)
        (i32.add)
        (i32.const 10)
        (i32.add)
        (br 1)
      )
      ;; case 1
      (i32.const 20)
      (i32.add)
      (i32.const 20)
      (i32.add)
    )
    ;; case 2 (and default)
    (local.set $result2)
    (local.set $result1)
    
    (call $print.string (i32.const 170) (i32.const 8))
    (local.get $result1)
    (call $print.int)
    (call $print.string (i32.const 170) (i32.const 8))
    (local.get $result2)
    (call $print.int)
  )

  ;; Test 9: complex composition
  (func $complex (param i32 i32) (result i32 i32 i32)
    ;; returns (a+b, a-b, a*2)
    (local.get 0)
    (local.get 1)
    (i32.add)
    
    (local.get 0)
    (local.get 1)
    (i32.sub)
    
    (local.get 0)
    (i32.const 2)
    (i32.mul)
  )

  (func $test_complex (export "test_complex")
    (call $print.string (i32.const 208) (i32.const 28))
    (i32.const 10)
    (i32.const 3)
    (call $complex)
    
    (i32.add)      ;; 20 + 7 = 27
    (i32.mul)      ;; 27 * 13 = 351
    
    (call $print.string (i32.const 170) (i32.const 8))
    (call $print.int)
  )


  (func $main (export "main")
    (call $test_swap)
    (call $test_triple)
    (call $test_nested)
    (call $test_block)
    (call $test_loop)
    (call $test_if (i32.const 1))
    (call $test_if (i32.const 0))
    (call $test_branch (i32.const 1))
    (call $test_branch (i32.const 0))
    (call $test_br_table (i32.const 0))
    (call $test_br_table (i32.const 1))
    (call $test_br_table (i32.const 2))
    (call $test_complex)
  )
)