(module
  (func (export "f32.no_fold_lt_select_to_abs") (param $x f32) (result f32) (select (f32.neg (local.get $x)) (local.get $x) (f32.lt (local.get $x) (f32.const 0.0))))
  (func (export "f32.no_fold_le_select_to_abs") (param $x f32) (result f32) (select (f32.neg (local.get $x)) (local.get $x) (f32.le (local.get $x) (f32.const -0.0))))
  (func (export "f32.no_fold_gt_select_to_abs") (param $x f32) (result f32) (select (local.get $x) (f32.neg (local.get $x)) (f32.gt (local.get $x) (f32.const -0.0))))
  (func (export "f32.no_fold_ge_select_to_abs") (param $x f32) (result f32) (select (local.get $x) (f32.neg (local.get $x)) (f32.ge (local.get $x) (f32.const 0.0))))

  (func (export "f64.no_fold_lt_select_to_abs") (param $x f64) (result f64) (select (f64.neg (local.get $x)) (local.get $x) (f64.lt (local.get $x) (f64.const 0.0))))
  (func (export "f64.no_fold_le_select_to_abs") (param $x f64) (result f64) (select (f64.neg (local.get $x)) (local.get $x) (f64.le (local.get $x) (f64.const -0.0))))
  (func (export "f64.no_fold_gt_select_to_abs") (param $x f64) (result f64) (select (local.get $x) (f64.neg (local.get $x)) (f64.gt (local.get $x) (f64.const -0.0))))
  (func (export "f64.no_fold_ge_select_to_abs") (param $x f64) (result f64) (select (local.get $x) (f64.neg (local.get $x)) (f64.ge (local.get $x) (f64.const 0.0))))
)

