// 01 实验: pybind11 + CUDA 合体 —— Python 一行调用 GPU 向量加法
// 编译(两条命令都给, 注意 nvcc 版要加 --extended-lambda 和大cks支持):
//   c++ -O3 -shared -std=c++17 -fPIC $(python3 -m pybind11 --includes) add_host.cpp \
//       -o add$(python3-config --extension-suffix) -L/usr/local/cuda/lib64 -lcudart
// 测试: python3 -c "import add; print(add.add_gpu([1.0,2.0,3.0], 10.0))"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>          // std::vector ↔ Python list 自动互转
#include <cuda_runtime.h>
#include <vector>
#include <stdexcept>

namespace py = pybind11;

// CUDA kernel: 每个元素加一个常数 (前置: cuda_handbook G02)
__global__ void add_kernel(const float* in, float* out, int n, float k) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) out[i] = in[i] + k;
}

// host 端包装: vector 进 → 显存算 → vector 出
// (工程化封装见 cpp_cuda_handbook X01/X02; 这里为了 lab 简洁用裸 API)
std::vector<float> add_gpu(std::vector<float> xs, float k) {
    int n = (int)xs.size();
    if (n == 0) return {};
    float *din, *dout;
    if (cudaMalloc(&din, n * 4) != cudaSuccess || cudaMalloc(&dout, n * 4) != cudaSuccess)
        throw std::runtime_error("cudaMalloc 失败");
    cudaMemcpy(din, xs.data(), n * 4, cudaMemcpyHostToDevice);
    add_kernel<<<(n + 255) / 256, 256>>>(din, dout, n, k);
    cudaMemcpy(xs.data(), dout, n * 4, cudaMemcpyDeviceToHost);
    cudaDeviceSynchronize();
    cudaFree(din); cudaFree(dout);
    return xs;                      // pybind11/stl.h 自动转回 Python list
}

PYBIND11_MODULE(add, m) {           // 模块名必须叫 add (和编译出的文件名一致)
    m.def("add_gpu", &add_gpu, py::arg("xs"), py::arg("k") = 1.0f,
          "GPU 向量加常数");
}
