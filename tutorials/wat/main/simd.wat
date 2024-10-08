(module
    (import "env" "print_int" (func $print.int (type $i32->void)))
    (type $void->void (func))
    (type $i32->void (func (param i32)))
    (memory 1)

    (func $run (type $void->void)
        (i64x2.splat (i64.const -9223372036854775808))
        drop
    )

    (export "main" (func $run))
)