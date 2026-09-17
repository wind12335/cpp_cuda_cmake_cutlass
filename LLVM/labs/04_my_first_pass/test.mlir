func.func @gemm_like(%a: f32, %b: f32, %c: f32) -> f32 {
  %t = arith.mulf %a, %b : f32
  %r = arith.addf %t, %c : f32
  return %r : f32
}
