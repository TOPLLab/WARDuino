(module
  ;; Type declarations
  (type $int32->int32->void (func (param i32 i32)))
  (type $int32->void (func (param i32)))
  (type $void->void (func))

  ;; Imports from the WARDuino VM
  (import "env" "chip_pin_mode" (func $env.chip_pin_mode (type $int32->int32->void)))
  (import "env" "print_int" (func $env.print_int (type $int32->void)))

  ;; Non-mutable globals
  (global $led i32 (i32.const 26))

  (func $init (type $void->void)  ;; Set pin mode function (private)
    global.get $led
    i32.const 2
    call $env.chip_pin_mode)

  (func $on (type $void->void)
    i32.const 42
    call $env.print_int)  ;; print 42


  (func $blink (type $void->void)  ;; Blink function (public)
    call $init  ;; initialise
    (i32.const 4)
    call_indirect (type $void->void) )

  (table 5 5 funcref)
  (elem (i32.const 4) func $on)

  ;; Export blink as function
  (export "main" (func $blink)))
