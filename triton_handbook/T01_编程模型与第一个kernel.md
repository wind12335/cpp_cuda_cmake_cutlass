# T01 Triton 编程模型与第一个 kernel

**【前置标注】** cuda_handbook G02（grid/block 概念）；你的 `triton-add.py`（本篇逐行重构它）。

**【记忆钩子】** CUDA 写**线程**，Triton 写**块**："一个 program 管一段数据"——`tl.arange` 切块、`mask` 防越界、`grid` 数量自动除出来。

**【人话版】** CUDA 里你要操心 1024 个线程各自算下标；Triton 里你只说"**我这个 program 负责 1024 个数**"，写的就是这 1024 个数的向量运算。一个 `tl.load` 背后编译器生成的是"一整个 warp 协作的数据搬运"。**你写的是逻辑，它编的是线程。**

---

## 一、名字 → 作用详解

| 名字 | 作用 | CUDA 对照 |
|---|---|---|
| `@triton.jit` | 装饰器：标记"这个 Python 函数是 GPU kernel"，首次调用时编译 | `__global__` |
| `tl.program_id(0)` | 当前 program 的编号（第几块） | `blockIdx.x` |
| `tl.arange(0, BLOCK)` | 生成长度 BLOCK 的下标向量 `[0,1,...,BLOCK-1]` | 你手写的 `i = threadIdx.x + ...` 一步到位 |
| `tl.load(ptr + offs, mask=...)` | 按下标向量**整块**读数据 | for 循环逐线程 ld.global（编译器生成合并访存）|
| `tl.store(ptr + offs, v, mask=...)` | 整块写回 | 同上 |
| `BLOCK: tl.constexpr` | 编译期常量（换值=重新编译一份）| `template<int BLOCK>` 值模板参数（C08.3！）|
| `triton.cdiv(N, BLOCK)` | 上取整除法 | `(N + BLOCK - 1) / BLOCK` |
| `kernel[grid](args...)` | **启动** kernel：grid 是"开几个 program" | `kernel<<<grid, block>>>(...)` |

**核心心智模型**：**你写的每一行都是"块级向量运算"**。`a_num + b_num` 不是两个数相加，
是两个 1024 维向量相加——编译器把它拆到 32×N 个线程上（SIMT），你不用管。

## 二、样例：逐行重构你的 triton-add.py（labs/01_vector_add.py）

```python
import torch
import triton
import triton.language as tl

@triton.jit
def vector_add_kernel(a_ptr, b_ptr, c_ptr,          # 指针: tensor 传进来自动变成指针
                      n_elements,                   # 运行期参数(普通值)
                      BLOCK_SIZE: tl.constexpr):    # 编译期参数(换值=重新编译)
    pid = tl.program_id(axis=0)                     # 我是几号 program(块)
    offs = pid * BLOCK_SIZE + tl.arange(0, BLOCK_SIZE)  # 我负责的下标段:
                                                     #   [pid*1024, pid*1024+1024)
    mask = offs < n_elements                        # 最后一块可能越界 → mask 防住
    a_num = tl.load(a_ptr + offs, mask=mask)        # 整块读 a
    b_num = tl.load(b_ptr + offs, mask=mask)        # 整块读 b
    c_num = a_num + b_num                           # 1024 维向量加法(编译器拆给线程)
    tl.store(c_ptr + offs, c_num, mask=mask)        # 整块写回 c

def solve(a, b, c, N):
    BLOCK_SIZE = 1024
    grid = (triton.cdiv(N, BLOCK_SIZE),)            # N=5000 → 开 5 个 program
    vector_add_kernel[grid](a, b, c, N, BLOCK_SIZE)
```

（你的原版逻辑全对，只有 `offests` 拼写、`solve` 里 `c` 参数被覆盖、main 太短三处小问题，labs 里已修。）

**mask 的必要性（你的 main 只测了 N=5 好运没炸）**：N=5000、BLOCK=1024 时第 5 块
负责 [4096,5120)，但只有 [4096,5000) 是合法的——没有 mask 就是越界写，随机崩。

## 三、踩坑实录

### ⚠️ 坑1：`def solve(a, b, c, N)` 里又写 `c = ...`——参数被覆盖

你的原版 `solve` 形参有 `c`，函数体里没用到它（结果直接存进了传入的 c）——运气对；
但如果函数体里出现 `c = 某新值`，就把指针参数覆盖了，kernel 写进了新 tensor，外面的 c 全是 0。

### ⚠️ 坑2：BLOCK_SIZE 不是越大越好

BLOCK 大 → 单个 program 干活多 → program 数少 → 并行度下降；BLOCK 小 → 启动开销占比大。
没有万能值——这正是 T04 autotune 存在的意义。

### ⚠️ 坑3：`tl.arange(0, BLOCK)` 的 BLOCK 必须是 2 的幂

`tl.arange(0, 1000)` 直接报错（要 16/32/.../1024/2048）。和 CUDA warp=32 对齐有关。

## 四、和 CUDA 的完整对照（贴墙背）

| Triton | CUDA (G02) |
|---|---|
| `@triton.jit` | `__global__ void` |
| `tl.program_id(0)` | `blockIdx.x` |
| `tl.arange(0,B)` + 算 offs | `int i = blockIdx.x*blockDim.x + threadIdx.x` |
| `mask` | `if (i < n)` 边界判断 |
| `tl.load/store` | 显式访存 + 你手动保证的合并访问 |
| `kernel[grid](...)` | `kernel<<<gridDim, blockDim>>>` |
| `BLOCK: tl.constexpr` | `template<int BLOCK>` |
| （没有的东西）| threadIdx / warp / shared memory 手写——**编译器代管** |

**人话版总结**：CUDA 是"包工头亲自动手"（线程级）；Triton 是"你画图纸，施工队（编译器）安排工人"（块级）。
图纸简单了，但你得懂施工队会怎么干活——这就是为什么先学 G 系列再学 Triton 事半功倍。

## 五、自测题

1. N=10000、BLOCK=1024，开几个 program？最后一个 program 的 mask 里几个 true？（10 个；10000-9216=784 个）
2. 把 BLOCK 从 1024 改成 128，程序行为哪里变？（program 数 10→79；每块搬运更小；性能可能变——T04 教你量）
3. 为什么 `tl.arange` 的参数必须是编译期常量？（编译器要静态知道向量长度来分配寄存器/生成指令——同 C08.3 值模板参数）
4. 你的 `n_elements` 为什么不写成 `tl.constexpr`？（它是运行期才知道的值；写成 constexpr 则每个 N 都重新编译一份 kernel，浪费）

动手：`cd labs && python 01_vector_add.py`，然后改 N=5000 观察第 5 块的 mask 行为（把 mask 打印出来试试）。
