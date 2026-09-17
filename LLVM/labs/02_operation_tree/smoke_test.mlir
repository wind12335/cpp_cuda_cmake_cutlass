// 一段手写的 MLIR：func dialect 里的一个函数
// 内容：return (x+x) + 0   ——  canonicalize 应该把 "+0" 消掉，把 x+x 变成 x*2
func.func @test(%x: f32) -> f32 {
  %0 = arith.constant 0.0 : f32
  %1 = arith.addf %x, %x : f32
  %2 = arith.addf %1, %0 : f32
  return %2 : f32
}
