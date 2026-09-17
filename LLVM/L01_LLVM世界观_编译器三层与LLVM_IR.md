# L01 LLVM 世界观：编译器三层与 LLVM IR

**【前置标注】** 无（这是编译器第一课）。会用命令行即可。

**【记忆钩子】** 前端管"听懂语言"，中端管"优化"，后端管"生成机器码"。LLVM 统一了中端——所有语言过同一条 LLVM IR 独木桥，再分流到各 CPU/GPU。

**【人话版】** 你写 `a+b`，C++ 写法和 Rust 写法长得不一样，但翻译成中间语言后一模一样。中间语言（IR，Intermediate Representation）就像世界语：N 种语言进，M 种硬件出，只需要 N+M 种翻译器，不用 N×M 种。

---

## 一、名字 → 作用详解

### 1. 编译器三层（所有编译器教材第一页）

| 层 | 干什么 | 例子 |
|---|---|---|
| 前端 Frontend | 词法/语法/语义分析，把源码变成 IR | clang（C/C++）、rustc 前端 |
| 中端 Middle-end | 在 IR 上做优化，与语言和硬件都无关 | LLVM 的 opt：死代码消除、内联、向量化 |
| 后端 Backend | 把优化后的 IR 变成汇编/机器码 | LLVM 的 llc：指令选择、寄存器分配 |

### 2. LLVM 是什么

一个**开源编译器基础设施**，核心资产是：
- **LLVM IR**：一种类似汇编但类型安全、平台无关的中间语言（`.ll` 文本 / `.bc` 二进制）
- 一整套在 IR 上做优化的 Pass（`opt` 工具）
- 一堆后端（x86/ARM/RISC-V/AMDGPU/NVPTX…）

**关键认知**：NVPTX 后端就是 CUDA 编译链的最后一环——`nvcc`/`clang` 把 CUDA 编译成 LLVM IR 再生成 PTX。你写的每个 kernel 都走过这条路。**Triton 的最后两步也是 LLVM**（L07 细讲）。

### 3. 工具链三兄弟（配 lab）

| 工具 | 输入 → 输出 | 作用 |
|---|---|---|
| `clang-15 -S -emit-llvm` | `.c` → `.ll` | 前端：生成人类可读的 LLVM IR |
| `opt-15` | `.ll` → `.ll` | 中端：跑优化 Pass |
| `llc-15` | `.ll` → `.s` | 后端：生成 x86 汇编 |

## 二、样例：亲手走完编译器三层（lab: `labs/01_llvm_pipeline/`）

```bash
cd labs/01_llvm_pipeline

# ① 前端：C → LLVM IR（-O0 不优化，看最朴素的 IR）
#    ⚠️ -Xclang -disable-O0-optnone 是本手册踩坑第一名，见下文"踩坑"
clang-15 -S -emit-llvm -O0 -Xclang -disable-O0-optnone matmul.c -o matmul_O0.ll

# ② 中端：IR 优化（O2 会对循环做自动向量化！）
opt-15 -S -O2 matmul_O0.ll -o matmul_O2.ll

# ③ 后端：IR → x86 汇编
#    ⚠️ -relocation-model=pic：不加的话偶发 PIE 链接错误（坑2，见下文）
llc-15 -O2 -relocation-model=pic matmul_O2.ll -o matmul_O2.s

# ④ 汇编 → 可执行文件 → 运行
clang-15 matmul_O2.s -o matmul_bin && ./matmul_bin   # 输出 c[0][1] = 2
```

打开 `matmul_O0.ll`（节选，注释是我加的）：

```llvm
; 一段三重循环矩阵乘的 LLVM IR。读法：
;   %开头 = 虚拟寄存器（SSA，每个名字只赋值一次，见 L02）
;   alloca = 栈上开空间（对应 C 的局部变量）
;   load/store = 读写内存
;   br = 跳转（label 当 goto 目标）
;   i32 = 32位整数，ptr = 指针
define dso_local void @matmul(i32 noundef %0, ptr noundef %1, ...) {
  %5 = alloca i32, align 4        ; 局部变量 i 的位置
  store i32 %0, ptr %5, align 4   ; 参数存进栈
  ...
  br label %12                    ; 循环开始
}
```

对比 `matmul_O2.ll` 里出现的（**已在本机实测**）：

```llvm
vector.body:                                      ; ← 自动向量化！
  %vec.phi = phi <4 x i32> [ ... ]                ; <4 x i32> = 4路SIMD
  ; 原来一次乘一个 int32，现在一条指令乘 4 个（SSE/AVX）
```

**这就是编译器存在的意义**：你没写一行 SIMD，O2 把三重循环自动变成向量指令。你 CUDA 手册里手动做的向量化（wmma/mma），是同样的问题在 GPU 上的版本——而 Triton/MLIR 的目标就是**让机器自动生成你手写的那些东西**。

## 三、踩坑实录（全部亲测）

### ⚠️ 坑1：`opt -O2` 之后 IR 毫无变化？

**现象**：`clang -S -emit-llvm -O0` 生成的 IR 过 `opt -O2`，函数体几乎没优化（还是一堆 alloca/load/store）。

**原因**：`-O0` 编译的函数自带 `optnone` 属性（"别优化我"，调试器需要）。`opt` 看到它直接跳过。

**解法**：前端生成时加 `-Xclang -disable-O0-optnone`。
或者干脆 `-O1` 以上生成（但那样你看到的第一份 IR 就已经优化过了，不利于学习）。

**验证**（本机实测）：不加这个 flag，`grep -c vector.body matmul_O2.ll` = 0；加了之后 = 7。

### ⚠️ 坑2：llc 出的汇编链接报 "can not be used when making a PIE object"

**现象**：`clang matmul_O2.s -o matmul_bin` 报 relocation 错误（有时只是 warning 有时直接失败——取决于字符串常量落位，很迷惑）。

**原因**：`llc` 默认生成非位置无关代码（静态程序假设），而 Ubuntu 的 clang 默认链接 PIE（位置无关可执行文件）。

**解法**：`llc-15 -O2 -relocation-model=pic`。

### ⚠️ 坑3：只有 `.c` 没有 main，链接报 `undefined reference to main`

纯函数文件链接成可执行文件需要 main。lab 里的 `matmul.c` 已带了 main，别删。

## 四、概念速查（背下来）

| 术语 | 一句话 |
|---|---|
| IR | 中间表示，源码和机器码之间的"世界语" |
| SSA | 静态单赋值：每个变量只赋值一次，`%1 = add %0, %0` 里 %0 永远不变。是现代编译器的地基，MLIR 全盘继承（L02） |
| Pass | 在 IR 上跑一遍、做一种变换的程序（优化器的基本单位）。**MLIR 学习 = 学写 Pass** |
| phi 指令 | SSA 下"控制流汇合处选值"的指令（`%x = phi [a, label1], [b, label2]`：从 label1 来取 a，从 label2 来取 b） |
| 向量化 | 把标量循环变成 SIMD 指令循环。CPU 上是 SSE/AVX，GPU 上 SIMT 天生就是（一个 warp 32 线程） |

## 五、自测题

1. 为什么有了 clang 还需要 opt 和 llc 两个工具？（提示：三层各自独立）
2. `-O0 -Xclang -disable-O0-optnone` 里，optnone 是谁加的、为什么要加？（调试器：断点/单步需要变量和代码一一对应）
3. 打开 `matmul_O0.ll`，找到最内层 `k` 循环的 `icmp`（比较）指令——它比较的是什么？
4. 预测：把 matmul.c 里的数组改成 `float`，O2 向量化后 `<4 x i32>` 会变成什么？（`<8 x float>`，AVX 一个寄存器 256bit 装 8 个 float）

## 六、下一步

- 动手：`cd labs/01_llvm_pipeline && bash run.sh`（脚本已写好全流程）
- 下一章 L02：MLIR 登场——为什么有了 LLVM IR 还要再造一个 IR？
