(module
  (func $main (result i32)
    (local $x i32)
    (local $y i32)
    i32.const 10
    local.set $x
    i32.const 20
    local.set $y
    local.get $x
    local.get $y
    i32.add
    return
  )
  (export "main" (func $main))
)
