func.func @cse_demo(%a: f32, %b: f32) -> f32 {
  %x = arith.mulf %a, %b : f32
  %y = arith.mulf %a, %b : f32    // 和 %x 一模一样 → CSE 后应消失
  %r = arith.addf %x, %y : f32
  return %r : f32
}
