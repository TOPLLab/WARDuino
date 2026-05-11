(module
  (type $T (func (result i32)))
  (table funcref (elem 0 1))

  (func $t1 (type $T) (i32.const 1))
  (func $t2 (type $T) (i32.const 2))

  (func (export "callt") (param $i i32) (result i32)
    (call_indirect (type $T) (local.get $i))
  )
)

