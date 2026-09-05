# 11 Tiling 为什么有效

## 面试口述版

GEMM 的痛点是**显存带宽喂不饱计算**：naive 版每算一个 C 元素要从显存读 A 的一行、B 的一列，
读 K×2 次数据只做 2 次浮点运算，算术强度≈1，完全被带宽锁死。
**Tiling（分块）**把 A 的一个 tile 和 B 的一个 tile 协作搬进 **shared memory**，block 内所有线程
反复复用它们：BM×BN 的 tile 做完，显存访问量从 O(K) 每元素降到 O(K/BK) 每元素——
数据复用率提高 BK 倍，算术强度提升一个量级，kernel 从"访存受限"爬上 roofline 的屋顶。
分块的层级感是面试亮点：**global→shared 的 block 级 tile → shared→register 的 thread 级 tile →
寄存器内再攒 8×8 微 tile**，每往里一层，数据被复用的次数就多一个量级。

## 高频追问

- **tile 多大合适？** 受三个约束：shared 容量（BM×BK + BK×N ≤ 100KB 量级）、每线程寄存器数、以及 tile 形状要让数据复用率够高——常见 128×128×8~32。大了占资源降 occupancy（联动 07），小了复用不够。
- **为什么不用 L2 就够了，还要 shared？** L2 是全卡共享的被动缓存，复用靠运气；shared 是 block 主动管理的显式复用，确定性高——"被动 cache vs 主动 tiling"是关键区分。
- **算术强度公式？** FLOPs / Bytes moved。GEMM 的 roofline 拐点在 ~300 FLOP/Byte（算：2×TFLOPS/带宽），tile 就是要把强度推过这条线。

## 动手绑定

| 做什么 | 位置 |
|--------|------|
| 读 naive → tiled 的完整演进 | `sgemm/sgemm.cu`（从 v1 到多版本递进，注释清晰）+ `interview/notes-v2.cu` Phase 7 SGEMM |
| FP16 对应版 | `hgemm/naive/hgemm.cu`（naive）——先读它再去看 mma 目录，感受"没 tile 的 FP16 一样惨" |
| 跑 bench 看差距 | `./notes_v2_cute_sm89.bin --bench-hgemm`（对照 naive 路径与 MMA 路径的 TFLOPS） |

## 记忆钩子

"naive 一个乘法一趟显存；tile 搬一次用 BK 轮；block→thread→寄存器，层层复用层层快。"

**人话版**：naive 的 GEMM 算一个乘加就要去显存拿一次数——搬运次数和计算次数一样多，全程在等显存。
**分块（tiling）**= 把 A、B 各切一小块一次性搬进 shared memory，之后 BK 轮计算都在片上复用这批
数据——搬一次，用 BK 次。而且复用分三层：block 级（global→shared）、线程级（shared→register）、
寄存器级（register 内攒多个结果），每进一层，同样的数据就被多用一个量级的次数。
