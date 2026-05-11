(assert_return (invoke "f32.no_fold_add_neg_neg" (f32.const -0.0) (f32.const -0.0)) (f32.const 0.0))
(assert_return (invoke "f32.no_fold_add_neg_neg" (f32.const 0.0) (f32.const -0.0)) (f32.const 0.0))
(assert_return (invoke "f32.no_fold_add_neg_neg" (f32.const -0.0) (f32.const 0.0)) (f32.const 0.0))
(assert_return (invoke "f32.no_fold_add_neg_neg" (f32.const 0.0) (f32.const 0.0)) (f32.const -0.0))

(assert_return (invoke "f64.no_fold_add_neg_neg" (f64.const -0.0) (f64.const -0.0)) (f64.const 0.0))
(assert_return (invoke "f64.no_fold_add_neg_neg" (f64.const 0.0) (f64.const -0.0)) (f64.const 0.0))
(assert_return (invoke "f64.no_fold_add_neg_neg" (f64.const -0.0) (f64.const 0.0)) (f64.const 0.0))
(assert_return (invoke "f64.no_fold_add_neg_neg" (f64.const 0.0) (f64.const 0.0)) (f64.const -0.0))


