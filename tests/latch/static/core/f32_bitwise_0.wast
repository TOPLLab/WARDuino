
(module
  (func (export "abs") (param $x f32) (result f32) (f32.abs (local.get $x)))
  (func (export "neg") (param $x f32) (result f32) (f32.neg (local.get $x)))
  (func (export "copysign") (param $x f32) (param $y f32) (result f32) (f32.copysign (local.get $x) (local.get $y)))
)

