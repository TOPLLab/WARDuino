;; This test program shows how to use the `heap_used` function 
;; to determine how much of the heap is currently in use.

;; The same used heap size can be obtained from the debugger, 
;; but note that the debugger introduces some overhead, so 
;; the value obtained there may be slightly higher 
;; than the value obtained from teh `heap_used` primitive.

(module 
  (import "env" "heap_used" (func $heap_used (result i32)))
  (import "env" "print_int" (func $print_int (param i32)))
  (memory (export "memory") 1)

  (func (export "main")
    (call $heap_used) ;; A
    (call $print_int)
  
    (memory.grow (i32.const 1))  ;; grow memory by 1 page (64KB = 65536 bytes)
    (drop)

    (call $heap_used) ;; should be >= A + 65536
    (call $print_int)
  )
)