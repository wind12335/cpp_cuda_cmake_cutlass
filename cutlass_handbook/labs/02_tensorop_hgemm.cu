// cutlass_handbook labs/02: 自己声明的 fp16 Tensor Core GEMM (sm_89)
// 编译运行:
//   CUTLASS=~/workspace/cuda_pybind_practice/third_party/cutlass
//   nvcc -std=c++17 -O3 -arch=sm_89 --expt-relaxed-constexpr \
//     -I$CUTLASS/include -I$CUTLASS/tools/util/include \
//     02_tensorop_hgemm.cu -o t02 && ./t02
#include <cstdio>
#include <vector>
#include <cuda_runtime.h>
#include "cutlass/gemm/device/gemm.h"
#include "cutlass/half.h"          // cutlass::half_t

// ───────── ① 声明 GEMM: 每个模板参数都在"选配置" ─────────
// A: fp16 行主序 | B: fp16 列主序 | C/D: fp16 行主序 (这就是"TN"布局, m16n8k16 的主场)
using Gemm = cutlass::gemm::device::Gemm<
    cutlass::half_t, cutlass::layout::RowMajor,       // A 的类型+布局
    cutlass::half_t, cutlass::layout::ColumnMajor,    // B 的类型+布局
    cutlass::half_t, cutlass::layout::RowMajor,       // C/D 的类型+布局
    float,                                            // 累加器与标量精度 (铁律: fp32)
    cutlass::arch::OpClassTensorOp,                   // 用 Tensor Core (不是 CUDA Core)
    cutlass::arch::Sm80,                              // ⚠ 实测: 4.6.1 的 2.x 风格 device::Gemm
                                                      //   的 TensorOp 特化只提供到 Sm80;
                                                      //   Sm80 实现完整兼容 sm_89 硬件(Ada 支持
                                                      //   Ampere 全部特性), 直接跑没问题!
                                                      //   (用 Sm89 反而会报 incomplete type)
    cutlass::gemm::GemmShape<128, 128, 64>,           // ② block 级 tile (一次搬 64 深度)
    cutlass::gemm::GemmShape<64, 64, 64>,             // ③ warp 级 tile
    cutlass::gemm::GemmShape<16, 8, 16>,              // ④ MMA 指令级 (m16n8k16)
    cutlass::epilogue::thread::LinearCombination<     // ⑤ epilogue: D = alpha*A*B + beta*C
        cutlass::half_t, 8, float, float>,
    cutlass::gemm::threadblock::GemmIdentityThreadblockSwizzle<>,  // ⑥ 输出块调度顺序
    3>;                                               // ⑦ 流水线级数 (shared 内存换吞吐)

int main() {
    const int M = 1024, N = 1024, K = 1024;
    // host 端 fp16 数据 (用 cutlass::half_t, 它就是能参与模板的 half)
    std::vector<cutlass::half_t> A(size_t(M) * K), B(size_t(K) * N);
    for (int i = 0; i < M * K; ++i) A[i] = cutlass::half_t(1.0f);
    for (int i = 0; i < K * N; ++i) B[i] = cutlass::half_t(1.0f);
    std::vector<cutlass::half_t> C(size_t(M) * N, cutlass::half_t(0.0f));

    cutlass::half_t *dA, *dB, *dC;
    cudaMalloc(&dA, A.size() * 2); cudaMalloc(&dB, B.size() * 2); cudaMalloc(&dC, C.size() * 2);
    cudaMemcpy(dA, A.data(), A.size() * 2, cudaMemcpyHostToDevice);
    cudaMemcpy(dB, B.data(), B.size() * 2, cudaMemcpyHostToDevice);
    cudaMemcpy(dC, C.data(), C.size() * 2, cudaMemcpyHostToDevice);

    // ───────── ② 传参: 尺寸 + 每个矩阵的 (指针, 主维) + 标量 ─────────
    // 行主序 A 的主维 lda = K (相邻两行隔 K 个元素); 列主序 B 的主维 ldb = K; 行主序 C 的 ldc = N
    float alpha = 1.0f, beta = 0.0f;
    Gemm gemm_op;
    cutlass::Status st = gemm_op({
        {M, N, K},
        {dA, K}, {dB, K}, {dC, N}, {dC, N},
        {alpha, beta}
    });
    if (st != cutlass::Status::kSuccess) { printf("GEMM 失败: %s\n", cutlassGetStatusString(st)); return 1; }

    // ───────── ③ 验证: 全 1 矩阵 × 全 1 矩阵 = 每个元素都是 K ─────────
    cudaMemcpy(C.data(), dC, C.size() * 2, cudaMemcpyDeviceToHost);
    float expect = float(K);
    double maxErr = 0;
    for (int i = 0; i < 16; ++i)
        for (int j = 0; j < N; j += (N / 16)) {
            double e = fabs(double(C[size_t(i) * N + j]) - expect);
            if (e > maxErr) maxErr = e;
        }
    printf("C[0][0]=%.1f (期望 %.0f), 抽样最大误差=%f %s\n",
           float(C[0]), expect, maxErr, maxErr < 1.0 ? "✓ Tensor Core GEMM 正确" : "✗");

    // ───────── ④ 顺手测一把性能 (event 计时, 前置: cuda_handbook G06/G08) ─────────
    cudaEvent_t t0, t1;
    cudaEventCreate(&t0); cudaEventCreate(&t1);
    cudaEventRecord(t0);
    for (int r = 0; r < 10; ++r) gemm_op({});
    cudaEventRecord(t1); cudaEventSynchronize(t1);
    float ms; cudaEventElapsedTime(&ms, t0, t1); ms /= 10;
    double tflops = 2.0 * M * N * K / (ms / 1000.0) / 1e12;
    printf("1024³ fp16 GEMM: %.3f ms = %.1f TFLOPS (%.0f%% of 4060 laptop 理论值)\n",
           ms, tflops, tflops / 30.0 * 100);

    cudaFree(dA); cudaFree(dB); cudaFree(dC);
    return 0;
}
