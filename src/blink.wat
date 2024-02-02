(module
  ;; Type declarations
  (type $int32->int32->void (func (param i32 i32)))
  (type $int32->void (func (param i32)))
  (type $int32->int32 (func (param i32) (result i32)))
  (type $void->void (func))

  ;; Imports from the WARDuino VM
  (import "env" "chip_delay" (func $env.chip_delay (type $int32->void)))
  (import "env" "print_int" (func $env.print_int (type $int32->void)))
  (import "env" "motor_test" (func $env.motor_test (type $int32->void)))
  (import "env" "drive_motor_for_ms" (func $env.drive_motor_for_ms (type $int32->int32->void)))
  (import "env" "drive_motor_degrees" (func $env.drive_motor_degrees (type $int32->int32->void)))
  (import "env" "chip_digital_read" (func $env.chip_digital_read (type $int32->int32)))
  (import "env" "chip_pin_mode" (func $env.chip_pin_mode (type $int32->int32->void)))
  (import "env" "chip_digital_write" (func $env.chip_digital_write (type $int32->int32->void)))

  ;; Non-mutable globals
  (global $led i32 (i32.const 45))
  (global $led2 i32 (i32.const 56))
  (global $led3 i32 (i32.const 39))
  (global $motora_pin5 i32 (i32.const 51))
  (global $motora_pin6 i32 (i32.const 50))
  (global $motora_pwm1 i32 (i32.const 36))
  (global $motora_pwm2 i32 (i32.const 37))
  (global $MOTORAB_SLEEP_N_IO i32 (i32.const 46))
  (global $power_supply_pin i32 (i32.const 60))
  (global $on i32 (i32.const 1))
  (global $off i32 (i32.const 0))

  ;; Initialise function (private)
  (func $init (type $void->void)
    ;; Set pin mode
    global.get $led
    i32.const 2
    call $env.chip_pin_mode
    global.get $led2
    i32.const 2
    call $env.chip_pin_mode
    global.get $led3
    i32.const 2
    call $env.chip_pin_mode
    global.get $motora_pin5
    i32.const 0
    call $env.chip_pin_mode
    global.get $motora_pin6
    i32.const 0
    call $env.chip_pin_mode
    
    global.get $power_supply_pin
    i32.const 2
    call $env.chip_pin_mode
    
    i32.const 1000
    call $env.chip_delay
    
    ;; Enable power supply for motors and such
    global.get $power_supply_pin
    global.get $off
    call $env.chip_digital_write  ;; turn led on
    
    
    ;; Enable motor controller
    global.get $MOTORAB_SLEEP_N_IO
    i32.const 2
    call $env.chip_pin_mode
    
    global.get $MOTORAB_SLEEP_N_IO
    global.get $on
    call $env.chip_digital_write  ;; turn led on
    
    ;; Set one pin to a duty cycle of 0%?
    ;;global.get $motora_pwm1
    ;;i32.const 2
    ;;call $env.chip_pin_mode
    
    ;;global.get $motora_pwm2
    ;;i32.const 2
    ;;call $env.chip_pin_mode
    
    ;;global.get $motora_pwm2
    ;;global.get $off
    ;;call $env.chip_digital_write  ;; turn led on
    
    ;;global.get $motora_pwm1
    ;;global.get $on
    ;;call $env.chip_digital_write  ;; turn led on
  )

  ;; Blink function (public)
  (func $blink (type $void->void)
    ;; Declare local $delay
    (local $delay i32)
    i32.const 1000
    local.set $delay

    ;; Initialise
    call $init
    
    global.get $led3
    global.get $on
    call $env.chip_digital_write  ;; turn led on

    ;; Blink in infinite loop
    loop $infinite
      global.get $led
      global.get $on
      call $env.chip_digital_write  ;; turn led on
      global.get $led2
      global.get $off
      call $env.chip_digital_write  ;; turn led 2 off
      local.get $delay
      call $env.chip_delay          ;; wait
      global.get $led
      global.get $off
      call $env.chip_digital_write  ;; turn led off
      global.get $led2
      global.get $on
      call $env.chip_digital_write  ;; turn led 2 on
      local.get $delay
      call $env.chip_delay          ;; wait
      
      global.get $motora_pin5
      call $env.chip_digital_read
      call $env.print_int
      
      global.get $motora_pin6
      call $env.chip_digital_read
      call $env.print_int
      
      ;;i32.const -775
      ;;call $env.motor_test
      ;;i32.const 1000
      ;;call $env.chip_delay          ;; wait
      ;;i32.const 0
      ;;call $env.motor_test
      
      ;;i32.const 1000
      ;;call $env.chip_delay          ;; wait
      
      ;;i32.const 775
      ;;call $env.motor_test
      ;;i32.const 1000
      ;;call $env.chip_delay          ;; wait
      ;;i32.const 0
      ;;call $env.motor_test
      
      ;;i32.const 3000                  ;; time
      ;;i32.const 775                   ;; speed
      ;;call $env.drive_motor_for_ms
      
      ;;i32.const 1000
      ;;call $env.chip_delay
      
      ;;i32.const 3000                  ;; time
      ;;i32.const -775                  ;; speed
      ;;call $env.drive_motor_for_ms
      
      i32.const 90
      i32.const 775
      call $env.drive_motor_degrees
      
      i32.const 1000
      call $env.chip_delay
      
      i32.const 90
      i32.const -775
      call $env.drive_motor_degrees
      
      br $infinite                  ;; jump back to start of loop
    end)

  ;; Export blink as function
  (export "main" (func $blink)))
