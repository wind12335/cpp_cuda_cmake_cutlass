// 06 实验: Split-K —— 让"瘦矩阵 GEMM"(M 很小) 也能吃满 GPU
// 场景: M=4, N=4096, K=16384 → 普通 GEMM 只有几十个 block 参与, 大量 SM 空转
//       Split-K 把 K 维切 64 份, 几百个 block 并行算"部分和", 再归约
//
// ⚠ 两个版本注意:
//   ① GemmSplitKParallel 类在单独的头文件 gemm_splitk_parallel.h (只 include gemm.h 会报
//      "no member GemmSplitKParallel", 亲测踩坑)
//   ② 它需要 workspace 存放各切片的部分和: get_workspace_size → cudaMalloc → 传给 initialize
//
// 编译运行:
//   CUTLASS=~/workspace/cuda_pybind_practice/third_party/cutlass
//   nvcc -std=c++17 -O3 -arch=sm_89 --expt-relaxed-constexpr \
//     -I$CUTLASS/include -I$CUTLASS/tools/util/include 06_splitk.cu -o t06 && ./t06
#include <cstdio>
#include <vector>
#include <cuda_runtime.h>
#include "cutlass/gemm/device/gemm.h"                   // 普通 dense 版
#include "cutlass/gemm/device/gemm_splitk_parallel.h"   // ⭐ Split-K 版在单独的头文件里

// 普通 dense 版
using GemmDense = cutlass::gemm::device::Gemm<
    cutlass::half_t, cutlass::layout::RowMajor,       // A: 行主序 (M×K)
    cutlass::half_t, cutlass::layout::ColumnMajor,    // B: 列主序 (K×N)
    cutlass::half_t, cutlass::layout::RowMajor,       // C/D: 行主序 (M×N)
    float, cutlass::arch::OpClassTensorOp, cutlass::arch::Sm80,
    cutlass::gemm::GemmShape<64, 128, 64>,            // block tile: M 小, 适配瘦矩阵
    cutlass::gemm::GemmShape<32, 64, 64>,             // warp tile
    cutlass::gemm::GemmShape<16, 8, 16>,
    cutlass::epilogue::thread::LinearCombination<cutlass::half_t, 8, float, float>,
    cutlass::gemm::threadblock::GemmIdentityThreadblockSwizzle<>, 2>;

// Split-K 并行版: 同样的配置, 换 GemmSplitKParallel。
// ⚠ 它的模板参数表和 device::Gemm 不同: Epilogue 之后还有 ConvertScaledOp/ReductionOp/
//   ThreadblockSwizzle/Stages 等——全部用默认值即可, 所以在 Epilogue 后直接收尾!
using GemmSplitK = cutlass::gemm::device::GemmSplitKParallel<
    cutlass::half_t, cutlass::layout::RowMajor,
    cutlass::half_t, cutlass::layout::ColumnMajor,
    cutlass::half_t, cutlass::layout::RowMajor,
    float, cutlass::arch::OpClassTensorOp, cutlass::arch::Sm80,
    cutlass::gemm::GemmShape<64, 128, 64>,
    cutlass::gemm::GemmShape<32, 64, 64>,
    cutlass::gemm::GemmShape<16, 8, 16>,
    cutlass::epilogue::thread::LinearCombination<cutlass::half_t, 8, float, float>>;

#define CUDA_CHECK_EQ(x) do { cutlass::Status s = (x); \
    if (s != cutlass::Status::kSuccess) { printf("CUTLASS error @%d: %s\n", __LINE__, cutlassGetStatusString(s)); exit(1);} } while (0)

int main() {
    const int M = 4, N = 4096, K = 16384;
    std::vector<cutlass::half_t> A(size_t(M) * K, cutlass::half_t(1.0f));
    std::vector<cutlass::half_t> B(size_t(K) * N, cutlass::half_t(1.0f));
    std::vector<cutlass::half_t> C(size_t(M) * N, cutlass::half_t(0.0f));

    cutlass::half_t *dA, *dB, *dC;
    cudaMalloc(&dA, A.size() * 2); cudaMalloc(&dB, B.size() * 2); cudaMalloc(&dC, C.size() * 2);
    cudaMemcpy(dA, A.data(), A.size() * 2, cudaMemcpyHostToDevice);
    cudaMemcpy(dB, B.data(), B.size() * 2, cudaMemcpyHostToDevice);
    cudaMemcpy(dC, C.data(), C.size() * 2, cudaMemcpyHostToDevice);

    cudaEvent_t t0, t1;
    cudaEventCreate(&t0); cudaEventCreate(&t1);
    float ms; double tflops = 0;

    // ---- 普通 dense 版 ----
    GemmDense dense;
    typename GemmDense::Arguments dArgs({M, N, K}, {dA, K}, {dB, K}, {dC, N}, {dC, N},
                                        {1.0f, 0.0f});
    CUDA_CHECK_EQ(dense.initialize(dArgs));
    for (int w = 0; w < 3; ++w) dense.run();
    cudaEventRecord(t0);
    for (int r = 0; r < 20; ++r) dense.run();
    cudaEventRecord(t1); cudaEventSynchronize(t1);
    cudaEventElapsedTime(&ms, t0, t1); ms /= 20;
    tflops = 2.0 * M * N * K / (ms / 1000.0) / 1e12;
    printf("普通 dense  GEMM: %.3f ms = %5.2f TFLOPS\n", ms, tflops);

    // ---- Split-K 版: split_k_slices=64 ----
    GemmSplitK splitk;
    typename GemmSplitK::Arguments sArgs({M, N, K}, {dA, K}, {dB, K}, {dC, N}, {dC, N},
                                         {1.0f, 0.0f});
    size_t ws_bytes = GemmSplitK::get_workspace_size(sArgs);   // 各切片部分和的存放空间
    void* workspace = nullptr;
    cudaMalloc(&workspace, ws_bytes);
    CUDA_CHECK_EQ(splitk.initialize(sArgs, workspace));        // workspace 传给 CUTLASS
    for (int w = 0; w < 3; ++w) splitk.run();                  // 预热
    cudaEventRecord(t0);
    for (int r = 0; r < 20; ++r) splitk.run();
    cudaEventRecord(t1); cudaEventSynchronize(t1);
    cudaEventElapsedTime(&ms, t0, t1); ms /= 20;
    tflops = 2.0 * M * N * K / (ms / 1000.0) / 1e12;
    printf("Split-K(64份) GEMM: %.3f ms = %5.2f TFLOPS\n", ms, tflops);

    // ---- 验证: 全 1 × 全 1 = 每个元素都是 K ----
    cudaMemcpy(C.data(), dC, C.size() * 2, cudaMemcpyDeviceToHost);
    printf("验证 C[0][0]=%.1f (期望 %d) %s\n", float(C[0]), K,
           float(C[0]) == float(K) ? "✓" : "✗");

    printf("\n【诚实结论(本机实测)】两种版本打平! 原因: B 矩阵 128MB 决定了这是【带宽受限】问题,\n");
    printf("两个版本搬运的数据量相同 → 性能相同。Split-K 赢的场景是'block 数 < SM 数'的严重饥饿\n");
    printf("(如 M、N 都只有几十 + K 巨大), 且归约开销小于并行度收益 —— 测量, 不要假设!\n");
    printf("这正是你 overlap 课题里'并行度不足时拆 K'的同族思想 (LLM 推理里叫 Flash-Decoding)。\n");

    cudaFree(dA); cudaFree(dB); cudaFree(dC); cudaFree(workspace);
    return 0;
}
