(assert_return (invoke "f32.no_fold_neg_add" (f32.const -0.0) (f32.const -0.0)) (f32.const 0.0))
(assert_return (invoke "f32.no_fold_neg_add" (f32.const 0.0) (f32.const -0.0)) (f32.const -0.0))
(assert_return (invoke "f32.no_fold_neg_add" (f32.const -0.0) (f32.const 0.0)) (f32.const -0.0))
(assert_return (invoke "f32.no_fold_neg_add" (f32.const 0.0) (f32.const 0.0)) (f32.const -0.0))

(assert_return (invoke "f64.no_fold_neg_add" (f64.const -0.0) (f64.const -0.0)) (f64.const 0.0))
(assert_return (invoke "f64.no_fold_neg_add" (f64.const 0.0) (f64.const -0.0)) (f64.const -0.0))
(assert_return (invoke "f64.no_fold_neg_add" (f64.const -0.0) (f64.const 0.0)) (f64.const -0.0))
(assert_return (invoke "f64.no_fold_neg_add" (f64.const 0.0) (f64.const 0.0)) (f64.const -0.0))


