(assert_return (invoke "func-unwind-by-br-value") (i32.const 9))
(assert_return (invoke "func-unwind-by-br_if-value") (i32.const 9))
(assert_return (invoke "func-unwind-by-br_table-value") (i32.const 9))
(assert_return (invoke "func-unwind-by-return") (i32.const 9))
(assert_return (invoke "block-unwind-by-br") (i32.const 9))
(assert_return (invoke "block-unwind-by-br-value") (i32.const 9))
(assert_return (invoke "block-unwind-by-br_if") (i32.const 9))
(assert_return (invoke "block-unwind-by-br_if-value") (i32.const 9))
(assert_return (invoke "block-unwind-by-br_table") (i32.const 9))
(assert_return (invoke "block-unwind-by-br_table-value") (i32.const 9))
(assert_return (invoke "block-unwind-by-return") (i32.const 9))
(assert_return (invoke "block-nested-unwind-by-br") (i32.const 9))
(assert_return (invoke "block-nested-unwind-by-br-value") (i32.const 9))
(assert_return (invoke "block-nested-unwind-by-br_if") (i32.const 9))
(assert_return (invoke "block-nested-unwind-by-br_if-value") (i32.const 9))
(assert_return (invoke "block-nested-unwind-by-br_table") (i32.const 9))
(assert_return (invoke "block-nested-unwind-by-br_table-value") (i32.const 9))
(assert_return (invoke "block-nested-unwind-by-return") (i32.const 9))
(assert_return (invoke "unary-after-br") (i32.const 9))
(assert_return (invoke "unary-after-br_if") (i32.const 9))
(assert_return (invoke "unary-after-br_table") (i32.const 9))
(assert_return (invoke "unary-after-return") (i32.const 9))
(assert_return (invoke "binary-after-br") (i32.const 9))
(assert_return (invoke "binary-after-br_if") (i32.const 9))
(assert_return (invoke "binary-after-br_table") (i32.const 9))
(assert_return (invoke "binary-after-return") (i32.const 9))
(assert_return (invoke "select-after-br") (i32.const 9))
(assert_return (invoke "select-after-br_if") (i32.const 9))
(assert_return (invoke "select-after-br_table") (i32.const 9))
(assert_return (invoke "select-after-return") (i32.const 9))
(assert_return (invoke "block-value-after-br") (i32.const 9))
(assert_return (invoke "block-value-after-br_if") (i32.const 9))
(assert_return (invoke "block-value-after-br_table") (i32.const 9))
(assert_return (invoke "block-value-after-return") (i32.const 9))
(assert_return (invoke "loop-value-after-br") (i32.const 9))
(assert_return (invoke "loop-value-after-br_if") (i32.const 9))
(assert_return (invoke "loop-value-after-br_table") (i32.const 9))
(assert_return (invoke "loop-value-after-return") (i32.const 9))
