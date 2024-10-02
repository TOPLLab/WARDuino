(module
    (import "env" "print_int" (func $print.int (type $i32->void)))
    (type $void->void (func))
    (type $i32->void (func (param i32)))
;;    (type $void->v128 (func (result v128)))

;;    (func $build (type $void->v128)
;;        (i32.const 0)
;;        i16x8.splat
;;        i32.const 1
;;        i16x8.replace_lane 0
;;        i32.const 2
;;        i16x8.replace_lane 1
;;        i32.const 3
;;        i16x8.replace_lane 2
;;        i32.const 4
;;        i16x8.replace_lane 3
;;        i32.const 5
;;        i16x8.replace_lane 4
;;        i32.const 6
;;        i16x8.replace_lane 5
;;        i32.const 7
;;        i16x8.replace_lane 6
;;        i32.const 8
;;        i16x8.replace_lane 7
;;    )

    (func $run (type $void->void)
        (i32.const 123456)
        i32x4.splat
        i32x4.extract_lane 0
        call $print.int

;;        call $build
;;        local.set $temp

;;        local.get $temp
        i32.const 0
        i16x8.splat
        i16x8.extract_lane_s 0
        call $print.int

;;        local.get $temp
        i32.const 0
        i16x8.splat
        i16x8.extract_lane_s 1
        call $print.int

;;        local.get $temp
        i32.const 0
        i16x8.splat
        i16x8.extract_lane_s 2
        call $print.int

;;        local.get $temp
        i32.const 0
        i16x8.splat
        i16x8.extract_lane_s 3
        call $print.int

;;        local.get $temp
        i32.const 0
        i16x8.splat
        i16x8.extract_lane_s 4
        call $print.int

;;        local.get $temp
        i32.const 0
        i16x8.splat
        i16x8.extract_lane_s 5
        call $print.int

;;        local.get $temp
        i32.const 0
        i16x8.splat
        i16x8.extract_lane_s 6
        call $print.int

;;        local.get $temp
        i32.const 0
        i16x8.splat
        i16x8.extract_lane_s 7
        call $print.int
    )

    (export "main" (func $run))
)