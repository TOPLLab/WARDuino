(module
  (func (export "f32.no_fold_add_neg_neg") (param $x f32) (param $y f32) (result f32)
    (f32.add (f32.neg (local.get $x)) (f32.neg (local.get $y))))

  (func (export "f64.no_fold_add_neg_neg") (param $x f64) (param $y f64) (result f64)
    (f64.add (f64.neg (local.get $x)) (f64.neg (local.get $y))))
)

