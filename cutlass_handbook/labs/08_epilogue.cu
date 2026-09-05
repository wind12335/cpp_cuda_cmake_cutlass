// 08 实验: epilogue 定制 —— 把 ReLU/GELU "焊进" GEMM 的收尾阶段 (算子融合)
// 对比四个版本: 基线 / ReLU epilogue / GELU epilogue / 基线+独立 ReLU kernel
// 编译运行:
//   CUTLASS=~/workspace/cuda_pybind_practice/third_party/cutlass
//   nvcc -std=c++17 -O3 -arch=sm_89 --expt-relaxed-constexpr \
//     -I$CUTLASS/include -I$CUTLASS/tools/util/include 08_epilogue.cu -o t08 && ./t08
#include <cstdio>
#include <cmath>
#include <vector>
#include <cuda_runtime.h>
#include "cutlass/gemm/device/gemm.h"
#include "cutlass/half.h"
#include "cutlass/epilogue/thread/linear_combination_relu.h"    // ReLU 收尾
#include "cutlass/epilogue/thread/linear_combination_gelu.h"    // GELU 收尾

// A: 行主序 fp16 | B: 列主序 fp16 | C/D: 行主序 fp16 | Tensor Core | Sm80 实现(兼容 sm_89)
#define GEMM_COMMON cutlass::half_t, cutlass::layout::RowMajor, \
                    cutlass::half_t, cutlass::layout::ColumnMajor, \
                    cutlass::half_t, cutlass::layout::RowMajor, \
                    float, cutlass::arch::OpClassTensorOp, cutlass::arch::Sm80, \
                    cutlass::gemm::GemmShape<64, 128, 64>, \
                    cutlass::gemm::GemmShape<32, 64, 64>, \
                    cutlass::gemm::GemmShape<16, 8, 16>

// ① 基线 epilogue: D = alpha·A·B + beta·C (LabB 用的就是它)
using GemmBase = cutlass::gemm::device::Gemm<
    GEMM_COMMON,
    cutlass::epilogue::thread::LinearCombination<cutlass::half_t, 8, float, float>,
    cutlass::gemm::threadblock::GemmIdentityThreadblockSwizzle<>, 2>;

// ② ReLU epilogue: 输出阶段直接过 ReLU —— "GEMM+bias+ReLU 一体化"(LLM 推理标配)
using GemmRelu = cutlass::gemm::device::Gemm<
    GEMM_COMMON,
    cutlass::epilogue::thread::LinearCombinationRelu<cutlass::half_t, 8, float, float>,
    cutlass::gemm::threadblock::GemmIdentityThreadblockSwizzle<>, 2>;

// ③ GELU epilogue: Transformer/FFN 的激活函数直接融合进 GEMM
using GemmGelu = cutlass::gemm::device::Gemm<
    GEMM_COMMON,
    cutlass::epilogue::thread::LinearCombinationGELU<cutlass::half_t, 8, float, float>,
    cutlass::gemm::threadblock::GemmIdentityThreadblockSwizzle<>, 2>;

// 独立 ReLU kernel (对照组: 不融合的"多一趟显存读写"做法)
__global__ void reluKernel(cutlass::half_t* d, int n) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) d[i] = max(float(d[i]), 0.0f);
}

// CPU 参考的 GELU (精确 erf 版)
static double cpuGelu(double x) { return 0.5 * x * (1.0 + erf(x / std::sqrt(2.0))); }

int main() {
    const int M = 2048, N = 2048, K = 2048;
    // A 按行分正负: 偶数 16 行组全 +0.5, 奇数组全 -0.5 → acc = ±128, 正好测 ReLU 的 0/保留
    std::vector<cutlass::half_t> A(size_t(M) * K), B(size_t(K) * N);
    for (int i = 0; i < M; ++i)
        for (int k = 0; k < K; ++k)
            A[size_t(i) * K + k] = cutlass::half_t(((i / 16) % 2) ? -0.5f : 0.5f);
    for (size_t i = 0; i < B.size(); ++i) B[i] = cutlass::half_t(1.0f);

    cutlass::half_t *dA, *dB, *dC;
    cudaMalloc(&dA, A.size() * 2); cudaMalloc(&dB, B.size() * 2); cudaMalloc(&dC, (size_t)M * N * 2);
    cudaMemcpy(dA, A.data(), A.size() * 2, cudaMemcpyHostToDevice);
    cudaMemcpy(dB, B.data(), B.size() * 2, cudaMemcpyHostToDevice);

    cudaEvent_t t0, t1;
    cudaEventCreate(&t0); cudaEventCreate(&t1);
    float acc_ref = ((0 / 16) % 2 ? -0.5f : 0.5f) * K;   // 行 0 的期望累加值 = +128

    // ---- ① 基线 LinearCombination ----
    GemmBase base;
    base.initialize({{M, N, K}, {dA, K}, {dB, K}, {dC, N}, {dC, N}, {1.0f, 0.0f}});
    base.run();
    std::vector<cutlass::half_t> hC(M * N);
    cudaMemcpy(hC.data(), dC, M * N * 2, cudaMemcpyDeviceToHost);
    printf("基线    : C[0]=%+.1f C[16K]=%+.1f (期望 %+.0f / %+.0f)\n",
           float(hC[0]), float(hC[16 * N]), acc_ref, -acc_ref);

    // ---- ② ReLU epilogue: 融合 ----
    GemmRelu relu;
    relu.initialize({{M, N, K}, {dA, K}, {dB, K}, {dC, N}, {dC, N}, {1.0f, 0.0f}});
    relu.run();
    cudaMemcpy(hC.data(), dC, M * N * 2, cudaMemcpyDeviceToHost);
    printf("ReLU 融合: C[0]=%+.1f (正行保留) C[16K]=%+.1f (负行→0)\n",
           float(hC[0]), float(hC[16 * N]));

    // ---- ③ GELU epilogue: Transformer 激活融合 ----
    GemmGelu gelu;
    gelu.initialize({{M, N, K}, {dA, K}, {dB, K}, {dC, N}, {dC, N}, {1.0f, 0.0f}});
    gelu.run();
    cudaMemcpy(hC.data(), dC, M * N * 2, cudaMemcpyDeviceToHost);
    printf("GELU 融合: C[0]=%+.1f (gelu(128)≈128) C[16K]=%+.1f (gelu(-128)≈0)\n",
           float(hC[0]), float(hC[16 * N]));

    // ---- ④ 性能: 融合 ReLU vs GEMM + 独立 ReLU kernel ----
    int block = 256, grid = (M * N + block - 1) / block;
    // (事件 t0/t1 在前面已创建, 这里直接用)

    GemmRelu fused;                                     // 融合版: 一个 kernel 全干完
    typename GemmRelu::Arguments argsRelu({M, N, K}, {dA, K}, {dB, K}, {dC, N}, {dC, N}, {1.0f, 0.0f});
    fused.initialize(argsRelu);
    for (int w = 0; w < 3; ++w) fused.run();
    cudaEventRecord(t0);
    for (int r = 0; r < 20; ++r) fused.run();
    cudaEventRecord(t1); cudaEventSynchronize(t1);
    float msFused; cudaEventElapsedTime(&msFused, t0, t1); msFused /= 20;

    GemmBase plain;                                     // 不融合: GEMM + 单独 ReLU 一趟
    typename GemmBase::Arguments argsPlain({M, N, K}, {dA, K}, {dB, K}, {dC, N}, {dC, N}, {1.0f, 0.0f});
    plain.initialize(argsPlain);                        // ⚠ 各实例化的 Arguments 类型不同, 不能共用!
    for (int w = 0; w < 3; ++w) plain.run();
    cudaEventRecord(t0);
    for (int r = 0; r < 20; ++r) {
        plain.run();
        reluKernel<<<grid, block>>>(dC, M * N);         // 多一趟: 全部输出 读+写 一遍
    }
    cudaEventRecord(t1); cudaEventSynchronize(t1);
    float msSep; cudaEventElapsedTime(&msSep, t0, t1); msSep /= 20;
    printf("\nGEMM+独立ReLU kernel: %.3f ms\n", msSep);
    printf("ReLU 融合进 epilogue : %.3f ms  ← 省一趟 16MB 的显存读写\n", msFused);
    (void)t0;

    cudaFree(dA); cudaFree(dB); cudaFree(dC);
    return 0;
}
