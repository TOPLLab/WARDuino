(module
    (import "env" "print_int" (func $print.int (type $i32->void)))
    (type $void->void (func))
    (type $i32->void (func (param i32)))

    (func $run (type $void->void)
        v128.const i32x4 1 2 3 4
        i32x4.extract_lane 0
        call $print.int
    )

    (export "main" (func $run))
)