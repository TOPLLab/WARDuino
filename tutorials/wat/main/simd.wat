(module
    (import "env" "print_int" (func $print.int (type $i32->void)))
    (type $void->void (func))
    (type $i32->void (func (param i32)))
    (memory 1)

    (func $run (type $void->void)
        (v128.store (i32.const 0) (v128.const i16x8 -1 2 -3 4 -5 6 -7 8))

        (v128.load (i32.const 0))
        i16x8.extract_lane_s 0
        call $print.int

        (v128.load (i32.const 0))
        i16x8.extract_lane_s 1
        call $print.int

        (v128.load (i32.const 0))
        i16x8.extract_lane_s 2
        call $print.int

        (v128.load (i32.const 0))
        i16x8.extract_lane_s 3
        call $print.int

        (v128.load (i32.const 0))
        i16x8.extract_lane_s 4
        call $print.int

        (v128.load (i32.const 0))
        i16x8.extract_lane_s 5
        call $print.int

        (v128.load (i32.const 0))
        i16x8.extract_lane_s 6
        call $print.int

        (v128.load (i32.const 0))
        i16x8.extract_lane_s 7
        call $print.int
    )

    (export "main" (func $run))
)