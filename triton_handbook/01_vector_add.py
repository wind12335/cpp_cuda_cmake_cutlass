# labs/01_vector_add.py —— T01 配套: 第一个 kernel + mask 边界实验
# 运行: source ~/.venv/bin/activate && python 01_vector_add.py
import torch
import triton
import triton.language as tl


@triton.jit
def vector_add_kernel(a_ptr, b_ptr, c_ptr, n_elements,
                      BLOCK_SIZE: tl.constexpr):
    pid = tl.program_id(axis=0)                        # 几号 program(块)
    offs = pid * BLOCK_SIZE + tl.arange(0, BLOCK_SIZE)  # 我负责的下标段(2的幂!)
    mask = offs < n_elements                           # 尾块越界防护
    a = tl.load(a_ptr + offs, mask=mask)               # 整块读
    b = tl.load(b_ptr + offs, mask=mask)
    tl.store(c_ptr + offs, a + b, mask=mask)           # 整块写(向量加法)


def vector_add(a: torch.Tensor, b: torch.Tensor) -> torch.Tensor:
    c = torch.empty_like(a)
    N = a.numel()
    BLOCK = 1024
    grid = (triton.cdiv(N, BLOCK),)                    # N=5000 → 5 个 program
    vector_add_kernel[grid](a, b, c, N, BLOCK)
    return c


if __name__ == "__main__":
    torch.manual_seed(0)
    # ① 小规模正确性
    a = torch.tensor([1., 2., 3., 4., 5.], device='cuda')
    b = torch.tensor([10., 20., 30., 40., 50.], device='cuda')
    print("小规模:", vector_add(a, b))                  # [11,22,33,44,55]

    # ② 大规模(带尾巴): N=5000, BLOCK=1024 → 第5块只有 784 个合法位置
    N = 5000
    a = torch.randn(N, device='cuda')
    b = torch.randn(N, device='cuda')
    c = vector_add(a, b)
    ok = torch.allclose(c, a + b)
    print(f"大规模 N={N} (尾块 784/1024 有效): 与 torch 对照 {'✓ 通过' if ok else '✗ 失败'}")

    # ③ T01 自测1 动手验证: BLOCK 改成 128, program 数变化
    BLOCK = 128
    grid = (triton.cdiv(N, BLOCK),)
    c2 = torch.empty_like(a)
    vector_add_kernel[grid](a, b, c2, N, BLOCK)
    print(f"BLOCK=128 时 program 数 = {grid[0]} (BLOCK=1024 时是 {triton.cdiv(N,1024)})")
