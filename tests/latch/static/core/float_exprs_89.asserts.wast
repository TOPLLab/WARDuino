(assert_return (invoke "f32.contract2fma" (f32.const 1.0) (f32.const 1.0)) (f32.const 0.0))
(assert_return (invoke "f32.contract2fma" (f32.const 0x1.19999ap+0) (f32.const 0x1.19999ap+0)) (f32.const 0.0))
(assert_return (invoke "f32.contract2fma" (f32.const 0x1.333332p+0) (f32.const 0x1.333332p+0)) (f32.const 0.0))
(assert_return (invoke "f64.contract2fma" (f64.const 1.0) (f64.const 1.0)) (f64.const 0.0))
(assert_return (invoke "f64.contract2fma" (f64.const 0x1.199999999999ap+0) (f64.const 0x1.199999999999ap+0)) (f64.const 0.0))
(assert_return (invoke "f64.contract2fma" (f64.const 0x1.3333333333333p+0) (f64.const 0x1.3333333333333p+0)) (f64.const 0.0))


