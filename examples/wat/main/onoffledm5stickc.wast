(module
  (type $i32->void (func (param i32)))
  (type $i32->i32->void (func (param i32 i32)))
  (type $i32->i32->i32->void (func (param i32 i32 i32)))
  (type $void->i32 (func (result i32)))
  (type $void->void (func))
  (type $type1 (func (param i32 i32 i32 i32 i32)))

  (import "env" "chip_delay" (func $env.chip_delay (type $i32->void)))
  (import "env" "subscribe_interrupt" (func $env.subscribeInterrupt (type $i32->i32->i32->void)))
  (import "env" "chip_pin_mode" (func $env.chip_pin_mode (type $i32->i32->void)))
  (import "env" "chip_digital_write" (func $env.chip_digital_write (type $i32->i32->void)))

  (global $onBtn i32 (i32.const 37))
  (global $offBtn i32 (i32.const 39))
  (global $led i32 (i32.const 10))
  (global $on i32 (i32.const 0))
  (global $off i32 (i32.const 1))
  (global $status (mut i32) (i32.const 0))

  ;; needed for subscribe_interrupt
  (memory $memory 1)
  (table 2 funcref)
  (elem $e0 (i32.const 0) $turnOn $turnOff)

  (func $setupButton (type $void->void)
    global.get $led
    i32.const 2
    call $env.chip_pin_mode

    ;; register on Button
    global.get $onBtn
    i32.const 0 ;; Table idx of $turnOn
    i32.const 2 ;; trigger callback on CHANGE
    call $env.subscribeInterrupt

    ;; register off button
    global.get $offBtn
    i32.const 1 ;; Table idx of $turnOff
    i32.const 2 ;; trigger callback on CHANGE
    call $env.subscribeInterrupt)

  (func $turnOn  (type $type1)
      global.get $on
      global.set $status)

  (func $turnOff  (type $type1)
      global.get $off
      global.set $status)

  (func $main  (type $void->void)
    (call $setupButton)

    (loop $infinite
        global.get $led
        global.get $status
        call $env.chip_digital_write
        i32.const 1000
        call $env.chip_delay
      (br $infinite)))
  (export "main" (func $main))
  )
