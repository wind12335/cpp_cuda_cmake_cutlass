# labs/04_fused_softmax.py —— T05 配套: 融合行 softmax + 对照 torch 性能
# 运行: python 04_fused_softmax.py
import time
import torch
import triton
import triton.language as tl


@triton.jit
def softmax_kernel(x_ptr, out_ptr, stride_xm, stride_on,
                   n_cols, BLOCK_N: tl.constexpr):
    row = tl.program_id(0)                              # 一个 program 管一行
    cols = tl.arange(0, BLOCK_N)                        # BLOCK_N >= n_cols(整行进块)
    mask = cols < n_cols
    x = tl.load(x_ptr + row * stride_xm + cols, mask=mask,
                other=-float('inf'))                    # ← 单位元选择: max 归约填 -inf,
                                                         #   exp(-inf)=0 不污染分母
    num = tl.exp(x - tl.max(x, axis=0))                 # 减 max 防溢出(数值课老朋友)
    den = tl.sum(num, axis=0)
    y = num / den
    tl.store(out_ptr + row * stride_xm + cols, y, mask=mask)


def triton_softmax(x: torch.Tensor) -> torch.Tensor:
    n_rows, n_cols = x.shape
    BLOCK_N = triton.next_power_of_2(n_cols)            # tl.arange 要 2 的幂
    out = torch.empty_like(x)
    softmax_kernel[(n_rows,)](x, out, x.stride(0), out.stride(0),
                              n_cols, BLOCK_N)
    return out


def bench(fn, warmup=10, rep=50):
    for _ in range(warmup): fn()
    torch.cuda.synchronize()
    t0 = time.perf_counter()
    for _ in range(rep): fn()
    torch.cuda.synchronize()
    return (time.perf_counter() - t0) / rep


if __name__ == "__main__":
    torch.manual_seed(0)
    # ① 正确性(含大数防溢出检验: 手工放一个 1e4)
    x = torch.randn(256, 4096, device='cuda')
    x[0, 0] = 1e4                                        # 没减 max 的话这里会 inf→NaN
    ok = torch.allclose(triton_softmax(x), torch.softmax(x, dim=-1), atol=1e-5)
    print(f"正确性(含 1e4 防溢出位): {'✓' if ok else '✗'}")

    # ② 性能: 大矩阵下融合 vs torch 版
    x = torch.randn(4096, 4096, device='cuda')
    t_tri = bench(lambda: triton_softmax(x))
    t_tch = bench(lambda: torch.softmax(x, dim=-1))
    gbs = lambda t: 2 * x.numel() * 4 / t / 1e9         # 读一趟 + 写一趟
    print(f"triton 融合: {t_tri*1e6:8.1f} µs (有效带宽 {gbs(t_tri):6.1f} GB/s)")
    print(f"torch 版:   {t_tch*1e6:8.1f} µs (有效带宽 {gbs(t_tch):6.1f} GB/s)")
    print(f"4060 显存带宽参考: ~272 GB/s; 越接近越是'融合到带宽极限'(T05 账本)")

    # ③ 思考题动手: 把 other 改成 0.0 会怎样?(T05 自测2 —— 取消注释试试)
    # 提示: 越界位 exp(0-(-inf))=exp(inf)=inf → NaN
