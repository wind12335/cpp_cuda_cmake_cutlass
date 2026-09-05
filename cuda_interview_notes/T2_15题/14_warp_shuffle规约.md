# 14 Warp Shuffle 规约（__shfl_down_sync）

## 面试口述版

规约的最后一段（warp 内 32→1）用 shared memory 又慢又费：每步要写 shared、`__syncthreads()`、再读。
**warp shuffle 是寄存器级的线程间直接交换**：`__shfl_down_sync(mask, v, delta)` 让每个线程
直接拿到"自己 + delta 号线程"的值，5 步（delta=16,8,4,2,1）完成 32→1，
全程寄存器、无 shared、无同步原语——一步一个 ALU 指令周期。
三要素：**mask**（参与交换的线程掩码，全 warp 就 0xffffffff）、**变量**（每线程持有的部分和）、
**delta**（对折距离）；还有 `__shfl_up/bxor/xor/idx` 变体（蝴蝶规约用 xor 最优雅）。
配套的 `__activemask()`/同步语义要点：shuffle 要求参与线程**在一条指令上集合**，
divergent 分支里用要小心（联动 06 题）。Block 级规约 = 各 warp 先 shuffle 归到每 warp 一个数 →
写 shared → 第一个 warp 再 shuffle 一轮。

## 高频追问

- **为什么 shuffle 版没有 bank conflict？** 根本不过 shared memory，线程间走的是硬件寄存器交换网络。
- **float 求和顺序变了吗？** 变了（树形序），结果与 CPU 串行求和有细微浮点差——面试主动提这个体现严谨（NCCL 的 ring allreduce 也一样是树形/环形序）。
- **对 16 线程（半个 warp）的活跃子集怎么办？** mask 用 `__activemask()` 或准确写出参与线程掩码，delta 从 8 开始对折——细节答出来就是加分。
- **和 NCCL 的关系？** 你可以讲：NCCL ring kernel 内部同样有 warp 级归约/广播原语，思想同源，只是规模放大到跨卡。

## 动手绑定

| 做什么 | 位置 |
|--------|------|
| 读 warp/block 规约完整实现 | `interview/notes-v2.cu` Phase 1（Warp Reduce → Block Reduce，注释含 WHY） |
| 读独立版本 | `reduce/block_all_reduce.cu` + `dot-product/dot_product.cu`（点积 = 规约的直接应用） |
| 跑验证 | `./notes_v2_cute_sm89.bin`：BlockReduce / Dot 行 Max Err ~1e-6（浮点树形序的正常误差） |

## 记忆钩子

"对折五步 32 归一，寄存器里直接换手；mask 集合、delta 减半、shared 只在 warp 之间。"

**人话版**：warp 内 32 个数求和的最后一段，用 **warp shuffle** 指令：每一步每个线程把数直接
"递给"对面 16/8/4/2/1 步之外的同伴（对折五步，32 个数归成 1 个），全程在寄存器之间完成，
不碰 shared memory、不需要同步指令。三个要点：**mask**（参与交换的线程名单）、**delta 每步减半**
（16→8→4→2→1）、**shared memory 只在"不同 warp 之间"汇合结果时才需要**。
