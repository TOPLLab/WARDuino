       (assert_return (invoke "32_good1" (i32.const 0)) (f32.const 0.0))
       (assert_return (invoke "32_good2" (i32.const 0)) (f32.const 0.0))
       (assert_return (invoke "32_good3" (i32.const 0)) (f32.const 0.0))
       (assert_return (invoke "32_good4" (i32.const 0)) (f32.const 0.0))
       (assert_return (invoke "32_good1" (i32.const 65524)) (f32.const 0.0))
       (assert_return (invoke "32_good2" (i32.const 65524)) (f32.const 0.0))
       (assert_return (invoke "32_good3" (i32.const 65524)) (f32.const 0.0))
       (assert_return (invoke "32_good4" (i32.const 65524)) (f32.const 0.0))
       (assert_return (invoke "32_good5" (i32.const 65524)) (f32.const 0.0))
       (assert_return (invoke "32_good1" (i32.const 65525)) (f32.const 0.0))
       (assert_return (invoke "32_good2" (i32.const 65525)) (f32.const 0.0))
       (assert_return (invoke "32_good3" (i32.const 65525)) (f32.const 0.0))
       (assert_return (invoke "32_good4" (i32.const 65525)) (f32.const 0.0))
