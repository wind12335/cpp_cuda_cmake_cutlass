func.func @test(%x: i32) -> i32 {
  %0 = arith.constant 0 : i32
  %1 = arith.addi %x, %x : i32
  %2 = arith.addi %1, %0 : i32
  %3 = arith.muli %1, %1 : i32
  %4 = arith.addi %2, %3 : i32
  return %4 : i32
}
