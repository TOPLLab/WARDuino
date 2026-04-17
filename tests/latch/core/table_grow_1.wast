(module
  (table $t 0x10 funcref)
  (elem declare func $grow)
  (func $grow (export "grow") (result i32)
    (table.grow $t (ref.func $grow) (i32.const 0xffff_fff0))
  )
)