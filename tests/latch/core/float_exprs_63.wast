(module
  (func (export "f32.no_fold_neg_sub") (param $x f32) (param $y f32) (result f32)
    (f32.neg (f32.sub (local.get $x) (local.get $y))))

  (func (export "f64.no_fold_neg_sub") (param $x f64) (param $y f64) (result f64)
    (f64.neg (f64.sub (local.get $x) (local.get $y))))
)

