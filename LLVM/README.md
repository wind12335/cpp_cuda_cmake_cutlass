# LLVM/MLIR 学习手册（llvm_handbook）

> 为"AI芯片算子库开发工程师 + 编译器研发工程师"岗位定制。
> 岗位 JD 关键词：**Triton 编译器、MLIR + Linalg、算子融合、自动调优、代码生成、LLVM/TVM/XLA/TorchInductor**。
> 你已有的 CUDA/CUTLASS/C++ 功底全部用得上，本手册负责把缺的编译器那一环补齐。

## 这个文件夹里有什么

| 内容 | 位置 | 说明 |
|---|---|---|
| 手册正文 | `L00` ~ `L07` 共 8 篇 + README | 按顺序读 |
| 可运行实验 | `labs/01` ~ `labs/06` | 全部在本机验证过 |
| LLVM 源码 | `llvm-project/` | 浅克隆，只读用（重点读 `mlir/examples/toy/`） |
| 北航 buddy 团队教材 | `buddy-mlir/` | 知乎高赞回答推荐的入门仓库 |

## 环境（已装好、已验证）

- MLIR 15.0.7 + clang 15：`sudo apt install clang-15 lld-15 llvm-15-dev libmlir-15-dev mlir-15-tools`
- 常用命令：`mlir-opt-15`（跑 Pass）、`clang-15 -S -emit-llvm`（看 IR）、`opt-15`（跑 LLVM 优化）
- ⚠️ 没有从源码编译 LLVM：本机内存只有 7.6G，源码编译要几十小时且容易 OOM。
  apt 的预编译包足够完成全部学习实验。真需要源码编译时租云主机（32G 内存起步）。

## 学习顺序（和既有手册的关系）

```
L01 LLVM 世界观      ← 编译器三层是什么（新知识，从这里入门）
L02 MLIR 核心概念    ← Operation/Dialect（新知识）
L03 mlir-opt 实操    ← 亲手跑官方 Pass（10 分钟上手）
L04 第一个 Pass：抄写 ← C++ 写 Pass（用上 cpp_handbook 的功底）
L05 第二个 Pass：改写 ← RewritePattern（编译岗面试核心考点）
L06 Linalg 与算子融合 ← JD 里 "MLIR+Linalg" 的落地（重点章）
L07 生态全景与面试    ← Triton/XLA/TVM/国产GPU栈 + 面试话术（串起你的 CUDA 背景）
```

**方法论**（来自知乎 BobHuang 高赞回答，本手册的课程设计就是照它做的）：
**抄写 Pass → 改写 Pass → 自制 Pass**。L04 抄、L05 改，L06 之后你就有能力对着 toy 教程自制了。

## 五步学习法（和其他手册一致）

1. **预测**：跑实验前先猜输出
2. **对照**：跑出来和猜的哪里不一样
3. **破坏**：故意改坏一个参数，看报什么错
4. **追问自测**：每章末尾的自测题
5. **口述**：能给人讲明白 = 真会了

## 一键跑全部实验

```bash
cd labs && bash check_all.sh
```
