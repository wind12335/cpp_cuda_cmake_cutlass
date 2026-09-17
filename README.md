# C++ · CUDA · CMake · CUTLASS · LLVM/MLIR 学习手册

> 一名研究生在国产 GPU 生态课题（分布式大模型计算-通信重叠方向）中，
> 从 C++ 入门一路学到 CUTLASS 定制与 MLIR 编译器的**全部讲义 + 可运行实验代码**。
> 每一段示例代码都在真实硬件上编译运行过——**没有纸上谈兵的内容**。

---

## 🖥️ 验证环境

| 项 | 值 |
|----|----|
| GPU | RTX 4060 Laptop（Ada, **sm_89**, 24 SM, 8GB） |
| 工具链 | CUDA 12.8 / GCC 11.4 / CMake 3.22 |
| 系统 | WSL2 (Ubuntu 22.04) |

---

## 📚 仓库结构（10 本教材）

### LLVM/MLIR 手册（编译器方向，新增）

| 文件夹 | 内容 | 亮点实验（全部实测） |
|--------|------|---------------------|
| [`LLVM/`](./LLVM/) | **MLIR/LLVM 编译器** L00-L07：编译器三层 → Operation/Dialect → mlir-opt → **亲手写 Pass**（抄写版+RewritePattern 改写版）→ Linalg 自动 tiling/算子融合 → Triton/XLA/IREE 生态与面试话术 | 自制 Pass `addf x,x→mulf x,2` 生效；一行 `linalg.matmul` 自动降三层循环 + `--linalg-tile` 自动分块；浮点 `+0.0` 陷阱 |

### 六本参考手册（教科书体：名字 → 作用详解 → 可编译示例 → 踩坑 → 前置标注）

| 文件夹 | 内容 | 亮点实验（全部实测） |
|--------|------|---------------------|
| [`cpp_handbook/`](./cpp_handbook/) | **C++ 教科书** C00-C12：变量 → 指针/内存 → 容器 → 类 → 智能指针 → 移动语义 → 模板/CRTP → 并发 → 现代 C++ 速查 | 移动 vs 拷贝差 **百万倍**（100MB: 193ns vs 209ms） |
| [`cuda_handbook/`](./cuda_handbook/) | **CUDA 教科书** G00-G09：架构 → kernel → 合并访存 → 内存层级 → 规约 → 流事件 → wmma → 工具 → 速查表 | 行读 vs 列读差 2 倍；wmma 对照 CPU 全对 |
| [`cpp_cuda_handbook/`](./cpp_cuda_handbook/) | **C++×CUDA 结合部** X00-X06：显存 RAII → kernel 包装器 → 模板 functor → host 流水线 → mini 张量类毕业项目 | 异常路径显存**零泄漏** ✓ |
| [`cmake_handbook/`](./cmake_handbook/) | **CMake** K01-K05：最小工程 → 变量/缓存 → target 与 PUBLIC/PRIVATE → CUDA 模板（含 `find_package(CUDAToolkit)` 实战报错）→ 速查排错 | 2 个工程构建运行 ✓ |
| [`pybind11_handbook/`](./pybind11_handbook/) | **pybind11**：最小模块 → Torch 扩展/LeetCUDA 模式 | GPU 模块真实 `import` ✓ |
| [`cutlass_handbook/`](./cutlass_handbook/) | **CUTLASS 4.6.1 实战**：官方 GEMM 精读 → **自写 fp16 Tensor Core GEMM** → 三层 GemmShape ↔ 手写 hgemm → 布局/TensorRef → Split-K → **epilogue 定制** | 自写 GEMM **17.7 TFLOPS**（理论值 59%）✓ |

### 三套面试笔记（问答体，与手册互补）

| 文件夹 | 内容 |
|--------|------|
| [`cpp_interview_notes/`](./cpp_interview_notes/) | 38 题：T1 必考 16 + T2 高频 14 + T3 扫一眼 8，每题配可运行代码 |
| [`cuda_interview_notes/`](./cuda_interview_notes/) | 25 题：CUDA 八股 + 通信专题，绑定 LeetCUDA 实测 |
| [`os_net_interview_notes/`](./os_net_interview_notes/) | OS×7 + 网络×5，每题配微基准（全部实测出数字） |

入口：**[学习手册总览.md](./学习手册总览.md)** —— 全部资料的导航页。

---

## 🎯 按目标选路径

| 你的目标 | 路径 |
|---------|------|
| 补 C++ 基础 | `cpp_handbook` C01→C04，配合 labs 逐个跑 |
| 系统学 CUDA | `cpp_handbook` C06/C07 → `cuda_handbook` G01→G09 |
| 准备 AI Infra 面试 | 三套 `*_interview_notes` + `cpp_handbook` C11 速查 |
| 读 LeetCUDA / 写自己的 kernel 库 | `cpp_cuda_handbook` X 系列 → `cutlass_handbook` |

---

## 🧪 亮点实验（均为真实运行结果）

| 实验 | 结果 |
|------|------|
| CUTLASS fp16 Tensor Core GEMM（自写声明） | 1024³ = **17.7 TFLOPS**（理论值 59%），正确性 ✓ |
| 共享内存 vs 管道（IPC） | **快 80 倍**（100 vs 1.2 GB/s）——NCCL 单机 SHM 底座 |
| 零拷贝三级演进 | read+write 264ms → mmap 164ms → **sendfile 22ms** |
| 移动 vs 拷贝（100MB） | 拷贝 209ms，移动 **193ns**（百万倍） |
| 双流流水线（拷贝/计算重叠） | 54ms → 40.5ms（**省一趟显存往返**） |
| false sharing 隔离 | **1.67 倍**加速（64B 缓存行对齐） |
| 页缺失首次触碰 | 256MB：mmap 0.003ms → 首触 190ms（65538 次缺页） |
| Split-K vs 普通 GEMM（瘦矩阵） | 实测打平 —— **"教科书答案"也要测量**（诚实结论见文档） |

---

## 📖 使用约定

1. 每个知识点按 **名字 → 作用详解 → 可编译示例 → 踩坑 → 【前置标注】** 组织；
   【前置：C03 §2】表示"不懂先回看 C 手册第 3 篇第 2 节"；
2. **五步学习协议**：预测输出 → 跑代码对照 → 改代码破坏 → 追问自测 → 闭卷口述
   （详见 `cpp_handbook/C00`）；
3. "记忆钩子"是压缩口诀，下面都配了**人话版**解码；
4. 遇到讲得不到位的地方，按"文件名 + 原句"记录，逐个击破。

## ⚙️ 编译约定

```bash
# C++
g++ -std=c++17 -O2 -pthread xxx.cpp -o out && ./out
# CUDA (RTX 4060 = sm_89)
nvcc -std=c++17 -O2 -arch=sm_89 xxx.cu -o out && ./out
# CUTLASS (头文件库)
nvcc -std=c++17 -O3 -arch=sm_89 --expt-relaxed-constexpr \
    -I/path/to/cutlass/include xxx.cu -o out && ./out
```

---

## 📌 License & 说明

- 手册与代码仅用于学习交流；CUTLASS 相关内容基于 [NVIDIA/cutlass](https://github.com/NVIDIA/cutlass)（BSD-3）本地副本编写；
- 实测数字基于 WSL2 环境，裸机上量级一致、绝对值会有出入——**引用时请自己复测**；
- 持续更新中：bug 修复、新知识点补充会持续 commit。
