(module
  (func (export "f32.no_fold_neg_add") (param $x f32) (param $y f32) (result f32)
    (f32.neg (f32.add (local.get $x) (local.get $y))))

  (func (export "f64.no_fold_neg_add") (param $x f64) (param $y f64) (result f64)
    (f64.neg (f64.add (local.get $x) (local.get $y))))
)

