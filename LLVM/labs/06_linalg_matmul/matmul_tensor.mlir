func.func @matmul(%A: tensor<4x4xf32>, %B: tensor<4x4xf32>, %C: tensor<4x4xf32>) -> tensor<4x4xf32> {
  %0 = linalg.matmul ins(%A, %B: tensor<4x4xf32>, tensor<4x4xf32>) outs(%C: tensor<4x4xf32>) -> tensor<4x4xf32>
  return %0 : tensor<4x4xf32>
}
