(module
  (memory 1)
  (data (i32.const 0) "\00\00\00\00\00\00\a0\7f\01\00\d0\7f")

  (func (export "32_good1") (param $i i32) (result f32)
    (f32.load offset=0 (local.get $i))                   ;; 0.0 '\00\00\00\00'
  )
  (func (export "32_good2") (param $i i32) (result f32)
    (f32.load align=1 (local.get $i))                    ;; 0.0 '\00\00\00\00'
  )
  (func (export "32_good3") (param $i i32) (result f32)
    (f32.load offset=1 align=1 (local.get $i))           ;; 0.0 '\00\00\00\00'
  )
  (func (export "32_good4") (param $i i32) (result f32)
    (f32.load offset=2 align=2 (local.get $i))           ;; 0.0 '\00\00\00\00'
  )
  (func (export "32_good5") (param $i i32) (result f32)
    (f32.load offset=8 align=4 (local.get $i))           ;; nan:0x500001 '\01\00\d0\7f'
  )
  (func (export "32_bad") (param $i i32)
    (drop (f32.load offset=4294967295 (local.get $i)))
  )
)

