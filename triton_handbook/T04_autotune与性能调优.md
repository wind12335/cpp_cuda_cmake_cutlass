# T04 autotune 与性能调优：让机器替你试参数

**【前置标注】** T03（三个 BLOCK 是什么）；LLVM/L06（自动调优=tile 参数搜索——同一句话的两次出现）。

**【记忆钩子】** 装饰器 `@triton.autotune` + 一排候选配置 = **首次运行自动扫一遍，最快的那份缓存下来**。JD 里的"自动调优"落到实处就这一屏代码。

**【人话版】** 你在 CUTLASS 里手选 `GemmShape<128,128,32>` 靠经验；Triton 的做法是把候选摆出来（128×128、128×64、64×64…×不同 num_warps/num_stages），**第一次跑某组形状时全部试一遍，掐表留最快**，之后直接用缓存。你在 4060 上手测过"不同 BLOCK 性能差异"——autotune 就是把这件事自动化。

---

## 一、名字 → 作用详解

| 名字 | 作用 |
|---|---|
| `@triton.autotune(configs=[...], key=[...])` | 声明候选配置；`key` 是"什么参数变了要重扫"（通常是矩阵尺寸）|
| `triton.Config({'BLOCK_M':128,...}, num_warps=8, num_stages=3)` | 一个候选：tile 参数 + 并行/流水参数 |
| `num_warps` | 一个 program 拆成几个 warp（4/8 常见）——影响"块内并行度" |
| `num_stages` | 软件流水级数：load 和 compute 重叠几层（G06 双流思想的指令级版！）|
| `kernel[grid]` 里的 lambda grid | 让 grid 跟着每个候选的 BLOCK 自动变 |
| `kernel.best_config` | 扫完后查"机器替你选了哪份" |

## 二、样例（labs/03 的核心段，实测）

```python
configs = [
    triton.Config({'BLOCK_M': 128, 'BLOCK_N': 128, 'BLOCK_K': 64}, num_warps=8, num_stages=3),
    triton.Config({'BLOCK_M': 128, 'BLOCK_N':  64, 'BLOCK_K': 64}, num_warps=4, num_stages=4),
    triton.Config({'BLOCK_M':  64, 'BLOCK_N':  64, 'BLOCK_K': 64}, num_warps=4, num_stages=3),
    triton.Config({'BLOCK_M':  64, 'BLOCK_N': 128, 'BLOCK_K': 32}, num_warps=4, num_stages=4),
]

@triton.autotune(configs=configs, key=['M', 'N', 'K'])   # M/N/K 变了就重扫
@triton.jit
def matmul_kernel(...): ...

matmul_kernel[grid](a, b, c, M, N, K, ...)   # 第一次某尺寸: 逐个试+掐表; 之后: 直接用缓存
print(matmul_kernel.best_config)             # 4060 上它选了谁? 跑 lab 看真机答案
```

**key 的语义**：同尺寸的矩阵只扫一次（结果缓存）；换尺寸（4096² → 8192²）自动重扫——
因为最优 tile 跟问题形状有关（大矩阵吃得下大 tile）。

## 三、num_warps / num_stages 到底在调什么（G 系列知识回收）

- **num_warps**：一个 program（一个 C tile）由几个 warp 协作算。warp 多 → 单块算力足但
  寄存器/WL（warp 调度）竞争；warp 少 → 块算得慢但能开更多 program。**本质是"每块用多宽的部队"**。
- **num_stages**：K 循环里"预取"几层。`num_stages=3` ≈ 当前算第 i 块时，第 i+1、i+2 块的
  数据已经在路上（异步拷贝+寄存器/共享内存缓冲）——**这就是 G06 你手写双流重叠的自动版，
  也是你 overlap 论文思想的指令级亲戚**（计算与访存重叠）。

## 四、手动掐表姿势（lab 里的标准模板）

```python
def bench(fn, warmup=10, rep=50):
    for _ in range(warmup): fn()                    # 预热(编译+缓存效应)
    torch.cuda.synchronize()
    t0 = time.perf_counter()
    for _ in range(rep): fn()
    torch.cuda.synchronize()                        # 收尾同步! 没它全是假时间
    return (time.perf_counter() - t0) / rep
```

**⚠️ 最常见的假数据**：忘了 `torch.cuda.synchronize()`——CUDA 是异步启动的，不同步你
量到的是"发射时间"不是"执行时间"。

## 五、踩坑实录

### ⚠️ 坑1：autotune 的 key 漏了关键形状参数

`key=['M','N']` 但 K 会变 → 错误地复用旧配置。凡是影响 tile 选择的尺寸都进 key。

### ⚠️ 坑2：warmup 不足，首跑计入冷启动

第一次调用含 JIT 编译（几百 ms），必须 warmup 排除，否则对比全歪。

### ⚠️ 坑3：候选太多首次巨慢

每个候选=一次编译+试跑，20 个候选 × 0.5s = 首跑 10s（生产环境常见优化：预编译/缓存共享）。
TRITON_CACHE_DIR（T06）里存的就是这些编译产物——所以**重启程序不用重扫**。

## 六、自测题

1. autotune 在什么时机扫？扫完存哪？（每个 key 组合首次调用时；结果与编译产物都在 Triton cache）
2. num_stages=4 比 =2 一定快吗？（不一定——寄存器压力可能 spills；这就是它要进搜索空间的原因）
3. 你论文里的"计算通信重叠"和 num_stages 什么关系？（同一思想两个层次：kernel 间流级重叠 vs K 循环内访存-计算流水）
4. JD"自动调优"你现在能落地解释了吗？（tile/并行/流水参数的搜索空间 + 掐表缓存——L06 linalg 的 tile-sizes 搜索是同一件事的编译器版）

动手：跑 `labs/03_matmul.py`，看 4060 替你选了哪个 Config，改成你预测的"最优"手动对比成绩。
