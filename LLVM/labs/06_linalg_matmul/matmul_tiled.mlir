#map = affine_map<(d0, d1)[s0] -> (d0 * 4 + s0 + d1)>
module {
  func.func @matmul(%arg0: memref<4x4xf32>, %arg1: memref<4x4xf32>, %arg2: memref<4x4xf32>) {
    %c2 = arith.constant 2 : index
    %c4 = arith.constant 4 : index
    %c0 = arith.constant 0 : index
    scf.for %arg3 = %c0 to %c4 step %c2 {
      scf.for %arg4 = %c0 to %c4 step %c2 {
        scf.for %arg5 = %c0 to %c4 step %c2 {
          %0 = memref.subview %arg0[%arg3, %arg5] [2, 2] [1, 1] : memref<4x4xf32> to memref<2x2xf32, #map>
          %1 = memref.subview %arg1[%arg5, %arg4] [2, 2] [1, 1] : memref<4x4xf32> to memref<2x2xf32, #map>
          %2 = memref.subview %arg2[%arg3, %arg4] [2, 2] [1, 1] : memref<4x4xf32> to memref<2x2xf32, #map>
          linalg.matmul ins(%0, %1 : memref<2x2xf32, #map>, memref<2x2xf32, #map>) outs(%2 : memref<2x2xf32, #map>)
        }
      }
    }
    return
  }
}

