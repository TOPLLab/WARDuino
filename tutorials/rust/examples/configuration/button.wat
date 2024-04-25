(module
  (type $t0 (func (param i32 i32)))
  (type $t1 (func (param i32) (result i32)))
  (type $t2 (func (param i32 i32 i32)))
  (type $t3 (func (param i32 i32 i32 i32 i32)))
  (type $t4 (func))
  (import "env" "chip_pin_mode" (func $env.chip_pin_mode (type $t0)))
  (import "env" "chip_digital_write" (func $env.chip_digital_write (type $t0)))
  (import "env" "chip_digital_read" (func $env.chip_digital_read (type $t1)))
  (import "env" "subscribe_interrupt" (func $env.subscribe_interrupt (type $t2)))
  (func $f4 (type $t3) (param $p0 i32) (param $p1 i32) (param $p2 i32) (param $p3 i32) (param $p4 i32)
    i32.const 26
    i32.const 26
    call $f8
    i32.const 1
    i32.ne
    call $f7)
  (func $main (type $t4)
    i32.const 25
    i32.const 0
    call $f6
    i32.const 26
    i32.const 2
    call $f6
    i32.const 25
    i32.const 1
    i32.const 2
    call $f9
    loop $L0
      br $L0
    end)
  (func $f6 (type $t0) (param $p0 i32) (param $p1 i32)
    local.get $p0
    local.get $p1
    call $env.chip_pin_mode)
  (func $f7 (type $t0) (param $p0 i32) (param $p1 i32)
    local.get $p0
    local.get $p1
    call $env.chip_digital_write)
  (func $f8 (type $t1) (param $p0 i32) (result i32)
    local.get $p0
    call $env.chip_digital_read)
  (func $f9 (type $t2) (param $p0 i32) (param $p1 i32) (param $p2 i32)
    local.get $p0
    local.get $p1
    local.get $p2
    call $env.subscribe_interrupt)
  (table $T0 2 2 funcref)
  (memory $memory 1)
  (global $g0 (mut i32) (i32.const 2048))
  (global $__data_end i32 (i32.const 2048))
  (global $__heap_base i32 (i32.const 2048))
  (export "memory" (memory $memory))
  (export "main" (func $main))
  (export "__data_end" (global $__data_end))
  (export "__heap_base" (global $__heap_base))
  (elem $e0 (i32.const 1) func $f4))
