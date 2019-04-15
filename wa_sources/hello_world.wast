(module
 (import "esp8266" "blink" (func $blink (type $1))) 
 (type $0 (func (param i32) (param i32) (result i32)))
 (type $1 (func (param i32) (result i32)))

 (memory $0 256 256)


 (export "add" (func $add))

 (func $add (; 0 ;) (type $0) (param $x i32) (param $y i32) (result i32)
  (get_local $x)
  (call $blink)
  (get_local $y)
  (call $blink)
  (get_local $x)
  (get_local $y)
  (i32.add)
  (return))

)
