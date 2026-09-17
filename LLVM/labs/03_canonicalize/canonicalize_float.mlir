func.func @float_trap(%x: f32) -> f32 {
  %zero = arith.constant 0.0 : f32
  %one = arith.addf %x, %x : f32
  %two = arith.addf %one, %zero : f32
  return %two : f32
}
