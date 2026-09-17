#map = affine_map<(d0, d1)[s0, s1, s2] -> (d0 * s1 + s0 + d1 * s2)>
module {
  func.func @matmul(%arg0: tensor<4x4xf32>, %arg1: tensor<4x4xf32>, %arg2: tensor<4x4xf32>) -> tensor<4x4xf32> {
    %c4 = arith.constant 4 : index
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %0 = bufferization.to_memref %arg1 : memref<4x4xf32, #map>
    %1 = bufferization.to_memref %arg0 : memref<4x4xf32, #map>
    %2 = bufferization.to_memref %arg2 : memref<4x4xf32, #map>
    %3 = memref.alloc() {alignment = 128 : i64} : memref<4x4xf32>
    memref.copy %2, %3 : memref<4x4xf32, #map> to memref<4x4xf32>
    scf.for %arg3 = %c0 to %c4 step %c1 {
      scf.for %arg4 = %c0 to %c4 step %c1 {
        scf.for %arg5 = %c0 to %c4 step %c1 {
          %5 = memref.load %1[%arg3, %arg5] : memref<4x4xf32, #map>
          %6 = memref.load %0[%arg5, %arg4] : memref<4x4xf32, #map>
          %7 = memref.load %3[%arg3, %arg4] : memref<4x4xf32>
          %8 = arith.mulf %5, %6 : f32
          %9 = arith.addf %7, %8 : f32
          memref.store %9, %3[%arg3, %arg4] : memref<4x4xf32>
        }
      }
    }
    %4 = bufferization.to_tensor %3 : memref<4x4xf32>
    memref.dealloc %3 : memref<4x4xf32>
    return %4 : tensor<4x4xf32>
  }
}

