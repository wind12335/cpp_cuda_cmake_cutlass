# T05 融合 kernel：算子融合的 Triton 落地（softmax / swiglu / reduce）

**【前置标注】** T02（mask/other）；cutlass_handbook 08（epilogue 融合——同一思想的 CUDA 版）；你的 swiglu/reduce/relu 练习。

**【记忆钩子】** 融合 = "**中间结果不出显存**"。行 softmax 是教科书案例：一行三步（max→exp→sum→div）一个 kernel 干完，中间量全在寄存器。

**【人话版】** 不融合的世界：softmax = 4 个独立 kernel（max、sub、exp、sum/div），每个都要"整行读进显存再写出"。融合后一趟读完、寄存器里算完、一趟写回——**省的是 3 趟显存往返**，这正是 cutlass 08 里 epilogue 干的事，也是 JD"算子融合"的日常形态。

---

## 一、名字 → 作用详解

| 名字 | 作用 |
|---|---|
| `tl.max(x, axis=1)` | 沿某维归约最大（softmax 减指数溢出用）|
| `tl.exp` / `tl.log` / `tl.sqrt` | 数学函数（块级，逐元素）|
| `tl.sum(x, axis=1)` | 沿某维求和 |
| `tl.reduce(x, axis, combine_fn)` | 通用归约：你自己提供二元合并函数（你 reduce.py 里研究过的）|
| `tl.atomic_add(ptr, v)` | 原子加：多 program 往同一个地址累加（跨块归约的最后一步）|
| `keep_dims=True` | 归约后保留那一维（广播对齐用，`[:, None]` 的函数版）|

## 二、样例：行 softmax（labs/04，官方 02-fused-softmax 的注释版）

```python
@triton.jit
def softmax_kernel(x_ptr, out_ptr, stride_xm, stride_xn,
                   n_cols, BLOCK_N: tl.constexpr):
    row = tl.program_id(0)                       # 一个 program 管一行
    cols = tl.arange(0, BLOCK_N)
    mask = cols < n_cols
    x = tl.load(x_ptr + row*stride_xm + cols*stride_xn,
                mask=mask, other=-float('inf'))  # ← other=-inf: 越界位取max后为-inf,
                                               #   exp(-inf)=0 天然不影响 sum!
    num = tl.exp(x - tl.max(x, axis=0))          # 减 max 防溢出(数值课老朋友)
    den = tl.sum(num, axis=0)
    tl.store(out_ptr + row*stride_xm + cols*stride_xn, num / den, mask=mask)
```

**融合账本**（对比 torch 的多 kernel 版，实测见 lab）：

```
torch.softmax:  max/sum 各一趟显存读 + 中间张量落显存(大矩阵时严重)
本 kernel:     每行 1 读 1 写, max/exp/sum/div 全在寄存器 → 接近带宽极限
```

**other 的语义选择**（T02 坑1 的活用）：这里填 `-inf` 而不是 0——因为 softmax 里
"无效位"必须贡献 0 到 exp 里，`-inf` 经 exp 变 0；填 0 的话 exp(0)=1 会污染分母。
**单位元思维：加法归约填 0，max 归约填 -inf，乘法填 1。**

## 三、你的 reduce.py 里已经悟到的：tl.reduce + atomic_add

跨块归约（N 超过一个块）的两级结构（你注释里写对了，直接引用）：

```python
@triton.jit
def _add(a, b): return a + b          # 合并函数: 必须@jit过、二元、满足结合律

# 块内: 树形两两合并(不是顺序累加——并行树的形状)
part = tl.reduce(x, axis=0, combine_fn=_add)
# 块间: 多个 program 的部分和原子累加到一个地址
tl.atomic_add(out, part)
```

**和 G05 reduce 阶梯的关系**：你 G05 手写的六级阶梯（单线程→shared memory 树归约→原子），
Triton 里就这两行——**编译器替你生成了那套阶梯**。但 atomic 有竞争代价，
大规模时更优的是"部分和写数组 + 第二个 kernel 终归约"（G05 阶梯第⑤级的思想）。

## 四、融合的判断标准（什么时候值得融）

| 适合融合 | 不适合 |
|---|---|
| 相邻算子有**逐元素/按行**依赖（softmax/swiglu/layernorm）| 输出要被**重复读多次**（先算完更好，让 L2 命中）|
| 中间量大（省显存往返收益高）| 算子间需要全局信息（如 sort）|
| kernel 启动开销占比高（小矩阵）| 融合后寄存器溢出（单 kernel 太肥）|

**人话版**：融合的本质是"**让中间量住在寄存器里**"——寄存器住不下（超大中间量）
或住完就扔（还要反复用）就不值得。

## 五、踩坑实录

### ⚠️ 坑1：softmax 的 BLOCK_N 必须覆盖整行

行内归约需要"整行在同一个块里"——BLOCK_N ≥ n_cols（不够就得多 pass，结构都变了）。
LLM 时代的 4096/8192 隐藏维度正是 Triton 行 softmax 的舒适区。

### ⚠️ 坑2：atomic_add 的精度

fp16 原子累加精度崩；用 fp32 输出张量累加（同 G05 的 fp32 部分和）。

### ⚠️ 坑3：数值上忘了减 max

exp(88+) 溢出 inf → 全行 NaN。深度学习面试送分题，也是实操真坑。

## 六、自测题

1. 融合 softmax 比非融合快，省的具体是什么？（3 趟显存读 + 中间张量的显存写/读）
2. 为什么 other 必须是 -inf 而不是 0？（exp(-inf)=0 恰好是"无效"的单位元；exp(0)=1 会污染分母）
3. 你的 swiglu 练习里，融合省了几趟显存？（ naïvely silu(x)→写回→乘 y→写回：省 2 读 1 写量级）
4. tl.reduce 的 combine_fn 为什么要满足结合律？（并行树合并顺序不定，结合律保证结果与顺序无关——你 reduce.py 注释里自己写过的）

动手：`python labs/04_fused_softmax.py`（正确性 + 与 torch.softmax 的性能对比 + swiglu 变体）。
