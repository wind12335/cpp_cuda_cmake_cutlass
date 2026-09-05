// 05 实验: "布局 = 坐标→偏移的映射函数" —— CUTLASS 最核心的抽象, 纯 host 可跑
// 编译运行(纯 host, 不需要 GPU):
//   g++ -std=c++17 -O2 -I$CUTLASS/include 05_tensorref.cpp -o t05 && ./t05
//   ($CUTLASS=~/workspace/cuda_pybind_practice/third_party/cutlass)
#include <cstdio>
#include <vector>
#include "cutlass/tensor_ref.h"
#include "cutlass/layout/matrix.h"     // RowMajor / ColumnMajor 布局

int main() {
    const int M = 4, N = 8;
    // 一个 4×8 矩阵, 32 个 float 连续存放
    std::vector<float> storage(size_t(M) * N);
    for (size_t i = 0; i < storage.size(); ++i) storage[i] = float(i);   // 填 0,1,2,...

    // ─── 核心概念: Layout 是一个"坐标 (row, col) → 线性偏移"的映射函数 ───
    // RowMajor:  offset(row, col) = row * N + col   (一行内部连续)
    // ColumnMajor: offset(row, col) = row + col * M  (一列内部连续)
    // 同一块内存, 换一个 Layout = 换一种"解读方式" —— 数据一个字节都不用动!

    cutlass::layout::RowMajor    rowLayout    = cutlass::layout::RowMajor::packed({M, N});
    cutlass::layout::ColumnMajor colLayout    = cutlass::layout::ColumnMajor::packed({M, N});

    // TensorRef = 指针 + Layout = "知道怎么定位任意元素的引用"
    cutlass::TensorRef<float, cutlass::layout::RowMajor>    rowRef(storage.data(), rowLayout);
    cutlass::TensorRef<float, cutlass::layout::ColumnMajor> colRef(storage.data(), colLayout);

    // ① offset 演示: 同一个 (row=2, col=3) 坐标, 两种布局算出的偏移不同
    printf("(2,3) 的偏移:  RowMajor=%lld, ColumnMajor=%lld\n",
           (long long)rowRef.offset({2, 3}), (long long)colRef.offset({2, 3}));
    // RowMajor: 2*8+3 = 19;  ColumnMajor: 2 + 3*4 = 14

    // ② TensorRef.at(): 像访问二维数组一样读写
    rowRef.at({1, 5}) = 999.0f;                       // 行主序视角下改 (1,5)
    printf("storage 线性位置 %lld 的值 = %.0f  (改 '坐标' 其实就是改内存)\n",
           (long long)rowRef.offset({1, 5}), (double)storage[rowRef.offset({1, 5})]);

    // ③ 同一块内存用 ColumnMajor 解读 = 免费得到"转置视图"
    printf("同一块内存, 两种解读的前 8 个元素:\n");
    printf("  RowMajor    矩阵:\n");
    for (int i = 0; i < M; ++i) {
        printf("    ");
        for (int j = 0; j < N; ++j) printf("%5.0f", storage[rowRef.offset({i, j})]);
        printf("\n");
    }
    printf("  ColumnMajor 矩阵(转置视图, 内存未变):\n");
    for (int i = 0; i < M; ++i) {
        printf("    ");
        for (int j = 0; j < N; ++j) printf("%5.0f", storage[colRef.offset({i, j})]);
        printf("\n");
    }
    return 0;
}
