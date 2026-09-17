func.func @demo(%x: f32, %y: f32) -> f32 {
  %a = arith.addf %x, %x : f32    // x + x  →  应被改写
  %b = arith.addf %x, %y : f32    // x + y  →  不该被改写
  %c = arith.addf %b, %b : f32    // b + b  →  应被改写（连锁）
  return %c : f32
}
