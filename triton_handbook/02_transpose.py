# labs/02_transpose.py —— T02 配套: 二维块/stride/转置 + 合并访存思考
# 运行: python 02_transpose.py
import time
import torch
import triton
import triton.language as tl


@triton.jit
def transpose_kernel(a_ptr, b_ptr, M, N,
                     stride_am, stride_an,    # a: 行主序时 = (N, 1)
                     stride_bm, stride_bn,    # b: 行主序时 = (M, 1)
                     BLOCK_M: tl.constexpr, BLOCK_N: tl.constexpr):
    pid_m = tl.program_id(0)
    pid_n = tl.program_id(1)
    offs_m = pid_m * BLOCK_M + tl.arange(0, BLOCK_M)
    offs_n = pid_n * BLOCK_N + tl.arange(0, BLOCK_N)
    # ⚠️实测踩坑: 输入块(M×N)和输出块(N×M)各要【形状配对】的 mask!
    #   方块尺寸相同时(BLOCK_M==BLOCK_N)形状错误骗过编译器, 但语义错 → 结果悄悄错
    mask_in = (offs_m[:, None] < M) & (offs_n[None, :] < N)    # M×N 的 mask
    mask_out = (offs_n[:, None] < N) & (offs_m[None, :] < M)   # N×M 的 mask(转置过!)

    # 读: 按原布局取 M×N 块 (行内连续 → 读侧合并 ✓)
    a = tl.load(a_ptr + offs_m[:, None] * stride_am + offs_n[None, :] * stride_an,
                mask=mask_in, other=0.0)
    # 写: 行列坐标互换 + tl.trans 块内转置 (写侧同一块内的列不连续 → 写发散, 见 T02 §三)
    tl.store(b_ptr + offs_n[:, None] * stride_bn + offs_m[None, :] * stride_bm,
             tl.trans(a), mask=mask_out)


def triton_transpose(a: torch.Tensor) -> torch.Tensor:
    assert a.is_cuda and a.dim() == 2
    M, N = a.shape
    b = torch.empty((N, M), device=a.device, dtype=a.dtype)
    grid = (triton.cdiv(M, 32), triton.cdiv(N, 32))
    # ⚠️stride 按"沿哪个下标轴"传, 不是按 tensor 第几维!
    #   b 是 (N,M): 它的 stride(1)=1 才是"沿 m 走一步", stride(0)=M 是"沿 n 走一步"
    transpose_kernel[grid](a, b, M, N,
                           a.stride(0), a.stride(1),   # a: 沿m / 沿n
                           b.stride(1), b.stride(0),   # b: 沿m / 沿n(注意换了!)
                           BLOCK_M=32, BLOCK_N=32)
    return b


def bench(fn, warmup=10, rep=100):
    for _ in range(warmup): fn()
    torch.cuda.synchronize()                  # ⚠️ 没有同步量到的全是假时间(T04 §四)
    t0 = time.perf_counter()
    for _ in range(rep): fn()
    torch.cuda.synchronize()
    return (time.perf_counter() - t0) / rep


if __name__ == "__main__":
    torch.manual_seed(0)
    # ① 正确性
    a = torch.randn(64, 48, device='cuda')
    ok = torch.allclose(triton_transpose(a), a.t())
    print(f"正确性: 与 a.t() 对照 {'✓' if ok else '✗'}")

    # ② 性能: 4096×4096 fp32(128MB/tensor, 远超 32MB L2 → 量到真实显存带宽)
    #   ⚠️小矩阵的坑(实测): 2048²只有 16MB, 整个住进 L2 → 带宽虚高到 570GB/s(>硬件 272)
    M = N = 4096
    a = torch.randn(M, N, device='cuda')
    b = torch.empty(N, M, device='cuda')
    t_tri = bench(lambda: transpose_kernel[(triton.cdiv(M,32), triton.cdiv(N,32))](
        a, b, M, N, a.stride(0), a.stride(1), b.stride(1), b.stride(0), 32, 32))
    t_tch = bench(lambda: b.copy_(a.t()))              # 公平对照: 同样复用输出缓冲
    gbs = lambda t: 2 * M * N * 4 / t / 1e9            # 读一趟+写一趟
    print(f"triton 转置: {t_tri*1e6:7.1f} µs  (有效带宽 {gbs(t_tri):6.1f} GB/s)")
    print(f"torch  转置: {t_tch*1e6:7.1f} µs  (有效带宽 {gbs(t_tch):6.1f} GB/s)")
    print("4060 显存带宽 ~272 GB/s; 朴素转置写侧发散, 能到一半以上就不错(见 T02 §三)")
