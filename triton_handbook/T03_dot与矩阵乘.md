# T03 tl.dot 与矩阵乘：BLOCK_M/N/K 就是 GemmShape

**【前置标注】** T02（二维块/stride/mask）；cutlass_handbook 02/03（三层 GemmShape——本篇全程对照）；你的 `triton_general_GEMM.py`（已写得相当好，本篇主要"确认你全对+补最后一块"）。

**【记忆钩子】** `tl.dot(a, b)` = 一块 mma；三个 BLOCK_M/N/K = CUTLASS 的 GemmShape<M,N,K>；外层 for k 循环 = K 维分块累加——**你手动学的 GEMM 结构，Triton 一屏写完**。

**【人话版】** CUTLASS 里你手写 `GemmShape<128,128,32>` + 三级循环 + epilogue；Triton 里同样的结构就是：二维切块（T02 公式）+ `for k` 循环 + `tl.dot` + 累加器。**结构一模一样，只是从 C++ 模板变成了 Python。**

---

## 一、名字 → 作用详解

| 名字 | 作用 | CUTLASS 对照 |
|---|---|---|
| `tl.dot(a, b, acc)` | 块级矩阵乘：`acc += a @ b`（可带 acc 链式）| 一簇 mma.sync 指令 |
| `acc = tl.zeros(...fp32)` | fp32 累加器 | fp32 accumulate 惯例 |
| `BLOCK_M/N/K: tl.constexpr` | 三个 tile 尺寸 | `GemmShape<M,N,K>` |
| `tl.range(0, K, BLOCK_K)` | K 维分块循环（你写的）| mainloop 的 k 迭代 |
| `out_dtype=tl.float16` / `acc.to(tl.float16)` | 存回降精度 | epilogue 的数据类型 |

**你 GEMM 练习的骨架（确认全对，这就是标准结构）**：

```python
acc = tl.zeros([BLOCK_M, BLOCK_N], dtype=tl.float32)     # ① fp32 累加器
for k0 in tl.range(0, K, BLOCK_K):                        # ② K 维分块
    a = tl.load(a_ptrs, mask=..., other=0.0)              # ③ 读 M×K 块(越界补0)
    b = tl.load(b_ptrs, mask=..., other=0.0)              #    读 K×N 块
    acc = tl.dot(a, b, acc)                               # ④ 块乘累加
c = alpha * acc + beta * c_loaded                          # ⑤ epilogue(你的α/β版)
tl.store(c_ptrs, c.to(tl.float16), mask=...)              # ⑥ 降精度写回
```

**K 尾部 mask 的巧妙**（你已经用对了）：K=100、BLOCK_K=32 时最后一块只有 4 列有效，
`other=0.0` 把无效位填 0——**0 乘什么都得 0，加进 acc 无害**。这是"用单位元填边界"的经典技巧（同 reduce 的 other=0、max 的 other=-inf）。

## 二、tl.dot 的硬件真相（串你的 G07/wmma）

`tl.dot` 在 sm_89 上最终编译成 **mma 指令**（fp16 输入 / fp32 累加，你 G07 手写过的 m16n8k16 家族）。
Triton 做的事：把你声明的 BLOCK 拆成硬件 mma 的碎片 + 摆好寄存器布局。所以：

- 输入 fp16/bf16、累加 fp32 时最快（硬件原生路径）；
- BLOCK 太小（如 8×8）浪费 mma 吞吐；太大（512×512）寄存器放不下会 spills；
- **约束**：三个维度有最小值要求（fp16 下 M,N ≥ 16, K ≥ 16 量级），太小直接编译错。

## 三、grid 怎么开：二维 program 网格

```python
grid = lambda meta: (triton.cdiv(M, meta['BLOCK_M']), triton.cdiv(N, meta['BLOCK_N']))
kernel[grid](...)     # 开 ceil(M/BM) × ceil(N/BN) 个 program, 每个算一个输出块
```

（你的版本是手算 grid——都对；`lambda meta` 版本的好处是 autotune 换 BLOCK 时 grid 自动跟着变，T04 用。）

**每个 program 算一个 C 的 tile**——和 CUTLASS"一个 CTA 一个 tile"同构。

## 四、踩坑实录

### ⚠️ 坑1：acc 精度用了 fp16

`tl.zeros(..., tl.float16)` 累加几百项后精度崩（和 G07 你学的 fp32-acc 理由一模一样）。永远 fp32 累加。

### ⚠️ 坑2：alpha/beta 时机

你把 `C*beta` 放在循环外 load 一次——对；如果 C 也要 mask（M/N 尾块），load C 同样要 mask+other=0。

### ⚠️ 坑3：`tl.dot(a, b)` 里 a/b 顺序就是数学顺序

`(M×K) @ (K×N)`，不存在 CUDA 里 "TN/NT layout" 那套心智负担——stride 已经把布局抽象掉了
（CUTLASS 02 里 TN/NN/NT 的烦恼在 Triton 里变成"传对 stride"一件事）。

## 五、自测题

1. M=256,N=512,K=1024, BLOCK=128×128×64：开几个 program？每个 program 循环几圈？（2×4=8 个；16 圈）
2. 为什么 load A 的 mask 是 `m_mask[:, None] & k_mask[None, :]` 这个形状？（A 块是 M×K，行合格且列合格）
3. BLOCK_K 越大越好吗？（大→循环少但单块寄存器压力大；小→循环开销占比大——这正是 autotune 的搜索维度）
4. 对照你的 CUTLASS 02：`GemmShape<128,128,32>` 里三个数，在 Triton kernel 里对应哪三个 constexpr？（BLOCK_M, BLOCK_N, BLOCK_K——同一件事的两套语法）

动手：`python labs/03_matmul.py`（autotune 版 GEMM + 对照 torch.matmul 计时 + 正确性校验）。
