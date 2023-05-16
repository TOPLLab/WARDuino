(module
  (memory 1)
  (func (export "store") (param i32 i64)
    (i64.store align=4 (local.get 0) (local.get 1))
  )
  (func (export "load") (param i32) (result i32)
    (i32.load (local.get 0))
  )
)

