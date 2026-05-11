(module
  (func (export "point_four") (param $four f64) (param $ten f64) (result i32)
    (f64.lt (f64.div (local.get $four) (local.get $ten)) (f64.const 0.4)))
)

