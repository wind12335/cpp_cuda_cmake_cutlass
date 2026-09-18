# T02 访存进阶：mask / other / stride / 二维块 / 转置

**【前置标注】** T01；cuda_handbook G03（合并访存——本篇反复对照）；C03 指针算术。

**【记忆钩子】** 一维块管"段"，二维块管"瓦"；**stride 是"行距"**——`ptr + 行×行距 + 列`，二维世界只有这一个公式。

**【人话版】** 真实算子的数据是矩阵（二维）。Triton 的二维块 = `offs_m[:, None] * stride + offs_n[None, :]`——一个"广播"造出整块坐标。而 stride（步长）让你**不管 tensor 是行主序还是列主序**都能正确寻址——这正是你 GEMM 练习里那 6 个 `a_sm/a_sk/...` 参数的意义。

---

## 一、名字 → 作用详解

| 名字 | 作用 |
|---|---|
| `offs[:, None]` / `offs[None, :]` | 升维成列向量/行向量 → 广播出二维坐标网格（C01 二维数组寻址的向量化版）|
| `stride_am` 等 | 沿某维走一步要跳几个元素（行主序矩阵 stride_m=N, stride_n=1）|
| `mask`（二维）| `m_mask[:, None] & n_mask[None, :]`——行合格且列合格的位置才读写 |
| `other=0.0` | mask 为 false 的位置**读到的值**（不填是未定义；GEMM 里填 0 累加不受影响）|
| `tl.zeros([M, N], tl.float32)` | 造全零累加器（精度用 fp32 累加——同 G07 wmma 的 fp32 acc 惯例）|
| `x.to(tl.float16)` | 存回时降精度（load fp16 → 计算 fp32 → store fp16 是标准套路）|

**二维坐标生成的唯一公式**（背下来，GEMM/transpose/卷积全用它）：

```python
offs_m = pid_m * BLOCK_M + tl.arange(0, BLOCK_M)   # 行下标向量
offs_n = pid_n * BLOCK_N + tl.arange(0, BLOCK_N)   # 列下标向量
ptrs   = base + offs_m[:, None] * stride_m + offs_n[None, :] * stride_n
#              └──── 列向量(M×1) ────┘   └──── 行向量(1×N) ────┘
#              广播相加 → M×N 的坐标网格
```

## 二、样例：转置（labs/02_transpose.py，你的 triton_transpose.py 重构版）

```python
@triton.jit
def transpose_kernel(a_ptr, b_ptr, M, N,
                     stride_am, stride_an,       # a 的行距/列距(行主序: N 和 1)
                     stride_bm, stride_bn,
                     BLOCK_M: tl.constexpr, BLOCK_N: tl.constexpr):
    pid_m = tl.program_id(0);  pid_n = tl.program_id(1)
    offs_m = pid_m * BLOCK_M + tl.arange(0, BLOCK_M)
    offs_n = pid_n * BLOCK_N + tl.arange(0, BLOCK_N)
    mask = (offs_m[:, None] < M) & (offs_n[None, :] < N)   # 二维 mask: 行合格 且 列合格

    a = tl.load(a_ptr + offs_m[:, None]*stride_am + offs_n[None,:]*stride_an,
                mask=mask, other=0.0)                      # 从 a 按原布局读 M×N 块
    tl.store(b_ptr + offs_n[:, None]*stride_bn + offs_m[None,:]*stride_bm,
             tl.trans(a), mask=mask)                       # 行列互换的坐标 + tl.trans 转块
```

**两个看点**：
1. **stride 的威力**：这个 kernel 不改动就能转置行主序、列主序、甚至"大矩阵的子块"——
   布局信息全在 stride 参数里。这就是 PyTorch 的 `.t()`/`.stride()` 体系和 Triton 的接口。
2. `tl.trans(a)` 是块内转置（寄存器里转）；坐标互换是全局转置——两个都要。

## 三、合并访存在哪？（G03 知识的用武之地）

你不用写合并访存，但要知道**编译器替你保证了什么**：

- 同一块的 `offs` 连续 → `tl.load` 生成**合并的**全局内存访问（一条 128B 事务伺候一 warp）；
- **转置为什么难快**：读 a 时按行读（合并）✓，但写到 b 时同一块里的列在内存里**不连续**
  （写发散）✗——这就是你 G03 里"列读 45.2 GB/s vs 行读 93.8 GB/s"的写侧版本。
- 高速转置的标准解法（shared memory 分块转再写）在 Triton 里是编译器/`tl.trans` 的事，
  但**诊断能力**是你的：转置慢，先想"哪一侧的访问模式发散了"。

## 四、踩坑实录

### ⚠️ 坑1：`other` 不填 = 越界位置读到垃圾

`tl.load(ptr, mask)` 不给 other 时越界位置值未定义。GEMM 里 K 维尾部越界必须 `other=0.0`
（0 加进累加器无害）；但 softmax 之类**乘法/指数**场景要小心 0 也可能不对——按语义选。

### ⚠️ 坑2：stride 用错单位

stride 是"**元素个数**"不是"字节数"（Triton 自动乘 dtype 大小）。传 `tensor.stride()` 直接对。

### ⚠️ 坑3：`&` 不是 `and`

二维 mask 是 `(行条件) & (列条件)`——对**布尔张量**逐元素与。写 `and` 直接语法错。

### ⚠️ 坑4：转置时 mask 也要跟着转（labs/02 实测翻车实录）

转置 kernel 的输入块是 M×N、输出块是 N×M——**store 必须用形状配对的新 mask**
（`(offs_n[:,None] < N) & (offs_m[None,:] < M)`），不能复用 load 的 mask。
阴险之处：BLOCK_M==BLOCK_N 时两个 mask 形状都是 32×32，**编译器查不出来**，
只有正确性校验（`torch.allclose`）能抓到——所以每个 lab 都先校验再计时。

## 五、自测题

1. 行主序 8×8 矩阵，`stride(0)` 和 `stride(1)` 各是几？（8 和 1；`.t()` 后变 1 和 8）
2. 上面的转置 kernel，把读 a 和写 b 的坐标公式互换（不换 tl.trans），输出是什么？（还是转置！读的是 a 的"转置块"——两种写法等价，想想为什么）
3. mask 写成 `offs_m < M` 不广播，会发生什么？（形状是 M 维向量，和二维 ptr 形状不匹配，编译错）
4. 为什么 `tl.zeros` 用 fp32 而输入常是 fp16？（数值精度：fp16 累加 1024 项就吃紧；G07 mma 同款惯例）

动手：`python labs/02_transpose.py`（含正确性校验 + 大矩阵计时，对照 torch.t 的带宽）。
