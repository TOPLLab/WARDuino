(module
  (type $t0 (func (param i32 i32)))
  (type $t1 (func (param i32) (result i32)))
  (type $t2 (func (param i32 i32 i32)))
  (type $t3 (func (param i32 i32 i32 i32 i32)))
  (type $t4 (func))
  (type $t5 (func (param i32)))
  (type $t6 (func (result i32)))

  (import "env" "chip_delay" (func $env.chip_delay (type $t5)))
  (import "env" "chip_pin_mode" (func $env.chip_pin_mode (type $t0)))
  (import "env" "chip_digital_write" (func $env.chip_digital_write (type $t0)))
  (import "env" "chip_digital_read" (func $env.chip_digital_read (type $t1)))
  (import "env" "subscribe_interrupt" (func $env.subscribe_interrupt (type $t2)))
  (import "env" "chip_ledc_setup" (func $env.chip_ledc_setup (type $t2)))
  (import "env" "chip_ledc_attach_pin" (func $env.chip_ledc_attach_pin (type $t0)))
  (import "env" "chip_analog_write" (func $env.chip_ledc_analog_write (type $t2)))

  ;; Non-mutable globals
  (global $led i32 (i32.const 26))
  (global $up i32 (i32.const 25))
  (global $down i32 (i32.const 33))

  ;; Mutable globals
  (global $delta (mut i32) (i32.const 0))

  (func $decrease (type $t4) ;; Red button pin 25
      i32.const -127
      global.set $delta)

  (func $increase (type $t4) ;; green button pin 33
      i32.const 127
      global.set $delta)

  (func $setup (type $t4)
    ;; configure pin
    i32.const 0
    i32.const 5000
    i32.const 12
    call $env.chip_ledc_setup
    global.get $led
    i32.const 0
    call $env.chip_ledc_attach_pin
    ;; setup buttons
    global.get $up
    i32.const 2
    i32.const 2
    call $env.subscribe_interrupt
    global.get $down
    i32.const 1
    i32.const 2
    call $env.subscribe_interrupt)

  (func $main (type $t4)
    ;; brightness variable
    (local $brightness i32)
    i32.const 75
    local.set $brightness
    ;; setup hardware
    call $setup
    ;; main loop
    loop $L0

      ;; change brightness
      local.get $brightness
      global.get $delta
      i32.add
      local.set $brightness
      (if
        (i32.gt_s
          (local.get $brightness)
          (i32.const 254))
        (then
          i32.const 254
          local.set $brightness))

      (if (i32.lt_s
            (local.get $brightness)
            (i32.const 0))
        (then
          i32.const 0
          local.set $brightness))

      i32.const 0
      local.get $brightness
      i32.const 254
      call $env.chip_ledc_analog_write

      ;; resetting delta
      i32.const 0
      global.set $delta

      i32.const 100
      call $env.chip_delay
      br $L0
    end)

  (table $T0 3 funcref)
  (memory $memory 1)

  (global $g0 (mut i32) (i32.const 2048))
  (global $__data_end i32 (i32.const 2048))
  (global $__heap_base i32 (i32.const 2048))
  (export "memory" (memory $memory))
  (export "main" (func $main))
  (export "__data_end" (global $__data_end))
  (export "__heap_base" (global $__heap_base))
  (elem $e0 (i32.const 1) func $decrease)
  (elem $e1 (i32.const 2) func $increase))