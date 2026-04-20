(module
  (import "env" "mask_interrupts" (func $mask_interrupts (param i32 i32) (result i32)))
  (import "env" "unmask_interrupts" (func $unmask_interrupts (param i32) ))
  (import "env" "event_group_interrupt" (global $event_group_interrupt i32))
  
  (import "env" "test" (func $test (param i32)))
  
  (import "env" "print_int" (func $print.int (param i32)))

  (func $main (export "main")
    (local $key i32)
    (local $i i32)
    ;; mask interrupt with discarding and all event groups
    ;; NOTE/TODO: currently, temporarily disabling the interrupts returns in an error when resolving the event pushed by the test interrupt
    (local.set $key (call $mask_interrupts (global.get $event_group_interrupt) (i32.const 1)))

    ;; 20 loops: in each iteration the interrupt is triggered, but discarded because of the discarding mask
    (local.set $i (i32.const 0))
    (loop $loop
      (call $print.int (local.get $i))
      (call $test (i32.const <<<<<<TODO_PROVIDE_FIDX>>>>>>)) ;; trigger the interrupt 
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br_if $loop (i32.lt_s (local.get $i) (i32.const 20)))
    )

    ;; unmaks the interrupt using the returned key
    (call $unmask_interrupts (local.get $key))
  )

  (func $test_callback (export "test_callback") (param $payload i32)
    (call $print.int (i32.const 424242))
  )
)
