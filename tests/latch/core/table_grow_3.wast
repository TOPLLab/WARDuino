(module
  (table $t 0 10 externref)
  (func (export "grow") (param i32) (result i32)
    (table.grow $t (ref.null extern) (local.get 0))
  )
)