# 16 Tensor Core 是什么

## 面试口述版

Tensor Core 是 GPU 里**专做小矩阵乘的硬件单元**：一条指令完成一小块矩阵乘加，
如 MMA `m16n8k16`——A(16×16) × B(16×8) + C(16×8) → D，一条指令 4096 个 FMA。
对比 CUDA Core 逐标量 FMA，吞吐差 1-2 个数量级，还**省寄存器带宽**（操作数驻留在寄存器 frag 里复用。
**frag** = fragment，"寄存器碎片"——MMA 指令把 16×16、16×8 这样的小矩阵块摊到一组固定编号的
寄存器上，这一组寄存器就叫一个 frag；数据装进 frag 后，喂给 Tensor Core 不再走显存）。
编程三层接口，从易到难：
① **WMMA**（`nvcuda::wmma`，片段抽象，易写但灵活性一般）；
② **MMA PTX**（`mma.sync.aligned.m16n8k16...`，精确控制行列分片和寄存器映射，LeetCUDA 主力写法），
配套 **ldmatrix** 从 shared 一次喂 8×8 的 frag；
③ **WGMMA/TMA**（Hopper，warp group 级、可直读 shared，LeetCUDA 有 sm90 分支）。
精度语义必答：输入 fp16/bf16/int8，**累加用 fp32**（防溢出/精度崩），这决定了 frag 的 ABCD 类型组合。

## 高频追问

- **m16n8k16 怎么读？** M×N×K = 输出 16×8，内积维 16；A 是 16×16（fp16 当 8 个寄存器）、B 16×8、C/D 16×8（fp32 当 4 个）。
- **wmma 和 mma.ptx 选哪个？** 学习/快速验证用 wmma；追求极致（自定义 swizzle、pack 布局）用 mma.ptx——LeetCUDA 两套都有，可对照。
- **为什么 MMA 要求数据从 shared 经 ldmatrix 进寄存器？** ldmatrix 一条指令完成 8×8×2B 的转置感知加载，且与 swizzle 配合无 bank conflict（联动 17 题）。
- **和你的课题？** 训练时 GEMM 全在 Tensor Core 上跑，你做 overlap 时"计算段"的真实耗时就是 MMA kernel 的耗时——测 overlap 收益时别拿 CUDA Core 版的 GEMM 当基线。

## 动手绑定

| 做什么 | 位置 |
|--------|------|
| 读 wmma 版 | `hgemm/wmma/`（hgemm 目录下）——先读懂 frag 的 load/mma/store 三段式 |
| 读 MMA PTX 版 | `hgemm/mma/basic/hgemm_mma.cu`（m16n8k16 TN 布局）→ `hgemm_mma_stage.cu`（+多级流水） |
| 实测威力 | `./notes_v2_cute_sm89.bin --bench-hgemm`：CUDA Core 路径 vs MMA 路径的 TFLOPS 差距自己看（通常 5-10 倍） |

## 记忆钩子

"一条指令一锅矩阵；fp16 进 fp32 算账；wmma 省心，mma.ptx 露活，WGMMA 上 Hopper。"

**人话版**：Tensor Core = GPU 里专做**小矩阵乘**的硬件单元，一条指令算完一小块（如 16×8×16）。
精度规则：输入用 fp16/bf16，**累加结果用 fp32**（防精度崩）。编程三层接口：
**wmma**（封装好的易用版，省心）、**mma PTX**（手工控制每个寄存器放哪，性能上限高、写起来费劲）、
**WGMMA**（只有 Hopper/H100 这一代支持的新指令，4060 上跑不了）。
