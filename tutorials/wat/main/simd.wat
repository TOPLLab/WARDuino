(module
    (import "env" "print_int" (func $print.int (type $i32->void)))
    (type $void->void (func))
    (type $i32->void (func (param i32)))
    (type $v128->v128->v128->v128 (func (param v128) (param v128) (param v128) (result v128)))
    (type $v128->v128->i32->v128 (func (param v128) (param v128) (param i32) (result v128)))
    (type $v128->i16 (func (param v128) (result i32)))
    (type $v128->void (func (param v128)))
    (memory 1)

    (func $vmlaq_u16 (type $v128->v128->v128->v128)
        (i16x8.mul (local.get 0) (local.get 1))
        (i16x8.add (local.get 2))
    )

    (func $vsraq_n_u16 (type $v128->v128->i32->v128)
        (i16x8.shr_s (local.get 0) (local.get 2))
        (i16x8.add (local.get 1))
    )

    (func $print.v128 (type $v128->void)
        local.get 0
        i16x8.extract_lane_s 0
        call $print.int

        local.get 0
        i16x8.extract_lane_s 1
        call $print.int

        local.get 0
        i16x8.extract_lane_s 2
        call $print.int

        local.get 0
        i16x8.extract_lane_s 3
        call $print.int

        local.get 0
        i16x8.extract_lane_s 4
        call $print.int

        local.get 0
        i16x8.extract_lane_s 5
        call $print.int

        local.get 0
        i16x8.extract_lane_s 6
        call $print.int

        local.get 0
        i16x8.extract_lane_s 7
        call $print.int
    )

    (func $vaddvq_u16 (type $v128->i16)
        (i32x4.extadd_pairwise_i16x8_u (local.get 0))
        local.tee 0
        i32x4.extract_lane 0
        local.get 0
        i32x4.extract_lane 1
        local.get 0
        i32x4.extract_lane 2
        local.get 0
        i32x4.extract_lane 3
        i32.add
        i32.add
        i32.add
    )

    (func $run (type $void->void)
        (call $vaddvq_u16 (v128.const i16x8 1 2 3 4 5 6 7 8))
        call $print.int
    )

    (export "main" (func $run))
)