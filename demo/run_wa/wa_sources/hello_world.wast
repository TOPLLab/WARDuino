(module
 (; Arduino imports ;)

 (import "env" "chip_delay"         (func $delay    (type $3)))
 (import "env" "print_string"       (func $print    (type $3)))
 (import "env" "get"                (func $get      (type $1)))

 (; Type declarations ;)
 (type $1 (func (param i32) (param i32) (param i32) (result i32)))
 (type $2 (func (param i32) (param i32)             (result)))
 (type $3 (func (param i32)                         (result)))
 (type $4 (func (param)                             (result)))

 (memory $mem 1)
 (data (i32.const 0) "H\00e\00l\00l\00o\00 \00t\00h\00e\00r\00e\00.")

 (; Define one function ;)
 (export "main" (func $main))

 (; The wait function ;)
 (func $wait (type $4)
    (;  Delay time   ;)
    (i32.const 1000)
    (call $delay)
 )

 (func $main (type $4)
    (i32.const 0)
    (i32.const 20)
    (i32.const 30)
    (call $get)
    (call $print)
    (call $wait)
 )
)
