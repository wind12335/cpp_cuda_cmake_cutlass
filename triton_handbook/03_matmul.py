# labs/03_matmul.py —— T03/T04 配套: autotune 版 GEMM + 对照 torch.matmul
# 运行: python 03_matmul.py
import time
import torch
import triton
import triton.language as tl

# ── T04: autotune 候选(搜索空间): tile 参数 × 并行/流水参数 ──
configs = [
    triton.Config({'BLOCK_M': 128, 'BLOCK_N': 128, 'BLOCK_K': 64}, num_warps=8, num_stages=3),
    triton.Config({'BLOCK_M': 128, 'BLOCK_N':  64, 'BLOCK_K': 64}, num_warps=4, num_stages=4),
    triton.Config({'BLOCK_M':  64, 'BLOCK_N':  64, 'BLOCK_K': 64}, num_warps=4, num_stages=3),
    triton.Config({'BLOCK_M':  64, 'BLOCK_N': 128, 'BLOCK_K': 32}, num_warps=4, num_stages=4),
]


@triton.autotune(configs=configs, key=['M', 'N', 'K'])   # 尺寸变了自动重扫
@triton.jit
def matmul_kernel(a_ptr, b_ptr, c_ptr,
                  M, N, K,
                  sam, sak, sbk, sbn, scm, scn,          # 六个 stride: 布局全抽象掉
                  BLOCK_M: tl.constexpr, BLOCK_N: tl.constexpr, BLOCK_K: tl.constexpr):
    pid_m = tl.program_id(0)                              # 每个 program 算 C 的一个 tile
    pid_n = tl.program_id(1)
    offs_m = pid_m * BLOCK_M + tl.arange(0, BLOCK_M)
    offs_n = pid_n * BLOCK_N + tl.arange(0, BLOCK_N)
    offs_k = tl.arange(0, BLOCK_K)
    m_mask = offs_m < M
    n_mask = offs_n < N

    a_ptrs = a_ptr + offs_m[:, None] * sam + offs_k[None, :] * sak
    b_ptrs = b_ptr + offs_k[:, None] * sbk + offs_n[None, :] * sbn

    acc = tl.zeros([BLOCK_M, BLOCK_N], dtype=tl.float32)  # ① fp32 累加器(永远!)
    for k0 in range(0, K, BLOCK_K):                       # ② K 维分块循环
        k_mask = (k0 + offs_k) < K
        a = tl.load(a_ptrs, mask=m_mask[:, None] & k_mask[None, :], other=0.0)  # ③ 尾块补0
        b = tl.load(b_ptrs, mask=k_mask[:, None] & n_mask[None, :], other=0.0)
        acc = tl.dot(a, b, acc)                           # ④ 块乘累加(硬件mma)
        a_ptrs += BLOCK_K * sak                           # 指针沿 K 推进(比重算下标快)
        b_ptrs += BLOCK_K * sbk
    c_ptrs = c_ptr + offs_m[:, None] * scm + offs_n[None, :] * scn
    tl.store(c_ptrs, acc.to(tl.float16),                   # ⑥ 降精度写回
             mask=m_mask[:, None] & n_mask[None, :])


def triton_matmul(a, b):
    M, K = a.shape; K2, N = b.shape; assert K == K2
    c = torch.empty(M, N, device=a.device, dtype=torch.float16)
    grid = lambda meta: (triton.cdiv(M, meta['BLOCK_M']), triton.cdiv(N, meta['BLOCK_N']))
    matmul_kernel[grid](a, b, c, M, N, K,
                        a.stride(0), a.stride(1), b.stride(0), b.stride(1),
                        c.stride(0), c.stride(1))
    return c


def bench(fn, warmup=10, rep=20):
    for _ in range(warmup): fn()
    torch.cuda.synchronize()
    t0 = time.perf_counter()
    for _ in range(rep): fn()
    torch.cuda.synchronize()
    return (time.perf_counter() - t0) / rep


if __name__ == "__main__":
    torch.manual_seed(0)
    M = N = K = 1024
    a = torch.randn(M, K, device='cuda', dtype=torch.float16)
    b = torch.randn(K, N, device='cuda', dtype=torch.float16)

    c = triton_matmul(a, b)                               # 首次: autotune 扫全部候选
    ref = a.float() @ b.float()                           # fp32 参考答案
    err = (c.float() - ref).abs().max().item()
    print(f"正确性: 最大误差 {err:.3f} (fp16 合理范围 <0.5) {'✓' if err < 0.5 else '✗'}")

    t_tri = bench(lambda: triton_matmul(a, b))            # 已缓存, 直接最快配置
    t_tch = bench(lambda: torch.matmul(a, b))             # cuBLAS(手写库的天花板)
    tflops = lambda t: 2 * M * N * K / t / 1e12
    print(f"triton(autotune 后): {t_tri*1e3:6.2f} ms  {tflops(t_tri):5.1f} TFLOPS")
    print(f"torch.matmul(cuBLAS): {t_tch*1e3:6.2f} ms  {tflops(t_tch):5.1f} TFLOPS")
    print(f"4060 替你选的配置: {matmul_kernel.best_config}")   # T04: 机器选了谁?
    print("注: 接近 cuBLAS 就很优秀——它是 NVIDIA 手调库; 差距大就回 T04 调候选")
