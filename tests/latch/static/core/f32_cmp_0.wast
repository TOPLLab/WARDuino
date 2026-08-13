
(module
  (func (export "eq") (param $x f32) (param $y f32) (result i32) (f32.eq (local.get $x) (local.get $y)))
  (func (export "ne") (param $x f32) (param $y f32) (result i32) (f32.ne (local.get $x) (local.get $y)))
  (func (export "lt") (param $x f32) (param $y f32) (result i32) (f32.lt (local.get $x) (local.get $y)))
  (func (export "le") (param $x f32) (param $y f32) (result i32) (f32.le (local.get $x) (local.get $y)))
  (func (export "gt") (param $x f32) (param $y f32) (result i32) (f32.gt (local.get $x) (local.get $y)))
  (func (export "ge") (param $x f32) (param $y f32) (result i32) (f32.ge (local.get $x) (local.get $y)))
)

