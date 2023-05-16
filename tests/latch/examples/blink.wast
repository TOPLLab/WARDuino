(module
  ;; Type declarations
  (type $int32->int32->void (func (param i32 i32)))
  (type $int32->void (func (param i32)))
  (type $void->void (func))

  ;; Imports from the WARDuino VM
  (import "env" "chip_delay" (func $env.chip_delay (type $int32->void)))
  (import "env" "chip_pin_mode" (func $env.chip_pin_mode (type $int32->int32->void)))
  (import "env" "chip_digital_write" (func $env.chip_digital_write (type $int32->int32->void)))

  ;; Non-mutable globals
  (global $led i32 (i32.const 26))
  (global $on i32 (i32.const 1))
  (global $off i32 (i32.const 0))

  (func $init (type $void->void)  ;; Set pin mode function (private)
    global.get $led
    i32.const 2
    call $env.chip_pin_mode)

  (func $blink (type $void->void)  ;; Blink function (public)
    ;; Declare local $delay
    (local $delay i32)
    i32.const 1000
    local.set $delay

    call $init ;; initialise

    ;; Blink in infinite loop
    loop $infinite
      global.get $led
      global.get $on
      call $env.chip_digital_write  ;; turn led on
        local.get $delay
      call $env.chip_delay          ;; wait
      global.get $led
      global.get $off
      call $env.chip_digital_write  ;; turn led off
      local.get $delay
      call $env.chip_delay          ;; wait
      br $infinite                  ;; jump back to start of loop
    end)

  ;; Export blink as function
  (export "main" (func $blink)))
