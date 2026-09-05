// G07 实验: 最小 wmma 矩阵乘 (16x16x16 单 tile), 与 CPU 对照
// 编译运行: nvcc -std=c++17 -O2 -arch=sm_89 G07_wmma.cu -o g07 && ./g07
#include <cstdio>
#include <cuda_fp16.h>
#include <mma.h>

using namespace nvcuda;

#define M 16
#define N 16
#define K 16

// 一个 warp 负责 D(16x16) = A(16x16) * B(16x16)
// A 行主序, B 列主序 (即 B 存储时是 K 行 x N 列 的转置布局 —— TN 布局, 实战最常见)
__global__ void wmma16(half* a, half* b, float* c) {
    wmma::fragment<wmma::accumulator, 16, 16, 16, float> c_frag;
    wmma::fragment<wmma::matrix_a,    16, 16, 16, half,  wmma::row_major> a_frag;
    wmma::fragment<wmma::matrix_b,    16, 16, 16, half,  wmma::col_major> b_frag;

    wmma::fill_fragment(c_frag, 0.0f);                 // 累加器清零
    wmma::load_matrix_sync(a_frag, a, K);              // A: 16x16, 行距 K
    wmma::load_matrix_sync(b_frag, b, K);              // B: 按列距 K 解释
    wmma::mma_sync(c_frag, a_frag, b_frag, c_frag);    // C = A×B + C
    wmma::store_matrix_sync(c, c_frag, N, wmma::mem_row_major);   // 写回, 行距 N
}

int main() {
    half *ha = new half[M * K], *hb = new half[K * N];
    float* hc = new float[M * N];
    // 初始化 A: A[i][k] = k (行主序: 元素在 ha[i*K + k])
    for (int i = 0; i < M * K; ++i) ha[i] = __float2half((float)(i % K));
    // 初始化 B: 故意让 B[k][j] = j。
    // ⚠ wmma 声明的是 col_major → 元素 (k,j) 存在 hb[k + j*K] —— 初始化必须按这个布局写!
    //   (初版按行主序填、按 col_major 读, 数据被"转置理解", 结果恒为 sum k² = 1240 —— 真实踩坑)
    for (int j = 0; j < N; ++j)
        for (int k = 0; k < K; ++k) hb[k + j * K] = __float2half((float)j);
    // 期望结果: C[i][j] = Σ_k A[i][k]*B[k][j] = Σ_k k*j = 120*j

    half *da, *db; float* dc;
    cudaMalloc(&da, M * K * 2); cudaMalloc(&db, K * N * 2); cudaMalloc(&dc, M * N * 4);
    cudaMemcpy(da, ha, M * K * 2, cudaMemcpyHostToDevice);
    cudaMemcpy(db, hb, K * N * 2, cudaMemcpyHostToDevice);

    // 一个 warp 负责一个 16x16 tile → 1 warp 就够, 但 block 至少 32 线程
    wmma16<<<1, 32>>>(da, db, dc);
    cudaDeviceSynchronize();
    cudaMemcpy(hc, dc, M * N * 4, cudaMemcpyDeviceToHost);

    // CPU 对照: c[i][j] = sum_k a[i][k] * b[k][j]
    // ⚠ 注意 B 是 col_major 存储: 元素 (k,j) 在 hb[k + j*K] —— CPU 参照必须按同一种布局读!
    //   (初版这里读成 hb[k*N + j], 与 GPU 用的布局不一致, 误报误差 1240 —— 又一个布局坑)
    double maxErr = 0;
    for (int i = 0; i < M; ++i)
        for (int j = 0; j < N; ++j) {
            float ref = 0;
            for (int k = 0; k < K; ++k)
                ref += __half2float(ha[i * K + k]) * __half2float(hb[k + j * K]);
            double e = fabs(ref - hc[i * N + j]);
            if (e > maxErr) maxErr = e;
        }
    printf("c[0][0]=%.1f c[15][15]=%.1f (期望 0 与 1800), 最大误差 = %f %s\n",
           hc[0], hc[M * N - 1], maxErr, maxErr < 1e-2 ? "✓ (fp16 输入 fp32 累加)" : "✗");

    cudaFree(da); cudaFree(db); cudaFree(dc);
    delete[] ha; delete[] hb; delete[] hc;
    return 0;
}
