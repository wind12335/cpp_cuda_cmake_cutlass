# T06 编译链路：亲手捞出 TTIR / TTGIR / PTX（LLVM 之桥）

**【前置标注】** T01；LLVM/L01-L02（本篇是它的"实例预告"——看完本篇去学 L02"一切皆 Operation"会秒懂）。

**【记忆钩子】** Triton 编译五站：**Python AST → TTIR → TTGIR → LLVM IR → PTX → CUBIN**。前两站是 MLIR 方言——`MLIR_ENABLE_DUMP=1` 亲眼见，`~/.triton/cache` 里直接躺着文件。

**【人话版】** 你写 `.py`，GPU 只认机器码。中间每一站都是 L01 讲的"多层 IR"：TTIR 还是"块级"的世界（你的 program/BLOCK 原样保留），TTGIR 降到"线程级"（数据怎么分给 32 个线程一组），LLVM IR 通用优化，PTX 是你 G 系列见过的老朋友。**这一章 = 把 LLVM 手册 L07 的那张图变成你机器上的文件。**

---

## 一、五站地图（每站干什么）

| 站 | 是什么 | 谁的视角 | 你在哪见过 |
|---|---|---|---|
| Python AST | `@triton.jit` 函数的语法树 | Python | — |
| **TTIR** | Triton IR（**MLIR 方言**）：还是块级运算 | "一个 program 一段数据" | 你的 T01 心智模型 |
| **TTGIR** | Triton GPU IR（还是 MLIR）：降到线程/寄存器 | warp/layout | G07 的碎片世界 |
| LLVM IR | 通用中间表示（LLVM 手册 L01 主角）| 机器无关 | L01 的 .ll |
| PTX | NVIDIA 虚拟汇编 | 硬件 | G07/nvcc -ptx |
| CUBIN | 机器码 | sm_89 | cuobjdump |

**降级时每站丢掉什么**：TTIR→TTGIR 把"块"拆成"线程布局"（你不用管的部分在这里被决定）；
TTGIR→LLVM 丢掉所有 Triton 概念变成通用指令；PTX 之后交给 ptxas。

## 二、实操①：从缓存目录捞文件（最直观，labs/05）

Triton 把每一站的产物**存进缓存**（这也是 autotune 不用重扫的原因，T04 坑3）：

```bash
cd labs && python 01_vector_add.py          # 先随便编译一个 kernel
find ~/.triton/cache -name "*.ttir" | head -3
# 挑一个目录看五件套:
DIR=$(find ~/.triton/cache -name "*.ttir" | head -1 | xargs dirname)
ls $DIR    # 里面有 .ttir .ttgir .llir .ptx .json .cubin ...
head -30 $DIR/*.ttir                        # ← 你人生第一次肉眼看到 MLIR!
```

**你在 .ttir 里会看到的**（LLVM/L02 的预告片）：

```
module attributes {"ttg.num-warps" = 4 ...} {
  tt.func public @vector_add_kernel(...) {       ← tt.func: triton 方言的函数 op
    %offs = tt.make_range ...                     ← 块级操作原样保留
    %a = tt.load %ptr, %mask                      ← 你写的 tl.load
    %c = arith.addf %a, %b                        ← 老朋友 arith! (L03 实验过它)
    tt.store ...
  }
}
```

`tt.func`/`tt.load`/`arith.addf`——**"自定义方言 + 复用通用方言"正是 MLIR 的设计**，
你在 L02 学的 dialect 概念此刻有了实物。

## 三、实操②：MLIR_ENABLE_DUMP 看每一遍 Pass（编译器开发视角）

```bash
MLIR_ENABLE_DUMP=1 python 01_vector_add.py 2>&1 | grep -E "^\*|IR Dump" | head
# 或只看某个 kernel:
MLIR_ENABLE_DUMP=vector_add_kernel python 01_vector_add.py
```

输出是 LLVM 手册 L03 的 `--mlir-print-ir-after-all` 同款：**每个编译 Pass 前后的 IR 全打印**。
你会看到几十个 pass（canonicalize、布局推断、指令选择…）逐级把 tt 世界降到 llvm 世界——
这就是"MLIR 多级下降"在你机器上的直播。

## 四、和 LLVM 手册的对接表（学完本篇你已具备的感性认识）

| LLVM 手册概念 | 你在 Triton 亲眼见过的对应物 |
|---|---|
| L01 编译器多层 IR | 五站：TTIR→TTGIR→LLVM IR |
| L02 dialect（方言） | `tt.` / `ttg.` 前缀；复用 `arith.` |
| L02 "一切皆 Operation" | `tt.func`/`tt.load` 全是 op |
| L03 mlir-opt 跑 Pass | MLIR_ENABLE_DUMP 打印的每个 pass |
| L06 linalg.tile 自动分块 | BLOCK_M/N/K 由 autotune 注入再下降 |
| L07 Triton 编译链一节 | 本篇就是那一节的实操版 |

## 五、踩坑实录

### ⚠️ 坑1：改了 kernel 代码，缓存里还是旧的

缓存 key 含源码 hash，正常会自动重编；但如果你手动改了缓存目录名/或者两份代码文件名相同
内容不同偶发混淆——`rm -rf ~/.triton/cache` 一键重来（代价：全部重新编译）。

### ⚠️ 坑2：MLIR_ENABLE_DUMP 输出海啸

全开是几万行。先 `| grep` 过滤 kernel 名，或重定向到文件慢慢看。

### ⚠️ 坑3：.ptx 里的外国指令不慌

`mma.sync`/`ldmatrix`/`cp.async` 都是 G 系列/wgmma 清单里的老朋友——Triton 生成的
就是当年你手写的那类指令（4060 上看到 m16n8k16 系列就是 G07 的亲戚）。

## 六、自测题

1. TTIR 和 TTGIR 的分界线一句话？（块级世界 vs 线程级世界——"数据分给谁"在这步决定）
2. 为什么 Triton 选 MLIR 而不是直接生成 LLVM IR？（需要在块级做优化（融合/分块），LLVM IR 层这些信息已丢失——LLVM/L02"多层存在的意义"的活例）
3. `tt.load` 里的 `tt.` 是什么？（triton 方言前缀——dialect 命名空间，同 arith.func 的 arith）
4. 缓存目录里 .ttir 和 .ptx 谁更接近硬件？（.ptx；.ttir 是最靠近你源码的一站）

动手：`bash labs/05_dump_ir.sh`（一键：编译→捞缓存→展示 ttir/ttgir/ptx 头部→标注对照）。
**做完这题，LLVM 手册 L02 可以开卷了。**
