# 17 ldmatrix / Swizzle

## 面试口述版

这是 MMA kernel 的"最后一公里"难题：**Tensor Core 吃数据要快、shared memory 偏偏在关键布局上撞 bank**。
**ldmatrix**：一条指令从 shared 批量加载 8×8 的 fp16 片到寄存器 frag（还支持 .trans 转置加载），
专为喂 MMA 设计——比逐元素 `smem[...]` 读取少一个量级的地址指令。
但它要求 128-bit 对齐行加载，而转置/特定 tile 布局会让 warp 的访问模式撞 bank（联动 05 题）——
padding 又会破坏 ldmatrix 的对齐连续性。**Swizzle（异或重排）**破局：写入 shared 时按
`row ^ (col >> 3)` 这类**位异或**打乱列位置，读侧按同一映射还原——零额外存储、无对齐损失、
bank 冲突消失，纯靠"收发货双方约定同一个乱序"。
层次递进：`mma_simple_swizzle`（最小例子）→ hgemm 的 stage_swizzle 版（多级流水 + swizzle 全家桶）。
看懂的关键是画图：`print_swizzle_layout.py` 能把异或模式画出来。

## 高频追问

- **swizzle 和 padding 怎么选？** 能 padding 的普通访问就 padding（简单）；喂 ldmatrix/TMA 的 MMA 场景必须 swizzle（padding 破坏对齐连续加载）。
- **swizzle 后数据"乱"了，MMA 怎么知道顺序？** frag 的寄存器映射和 swizzle 模式是配套设计的——乱序发生在 shared 地址上，进 frag 后逻辑顺序正确。这也是手写 MMA 最绕的地方。
- **Hopper 的 TMA 和 swizzle？** TMA 硬件搬运时就支持 swizzle 模式（CUtensorMap 里配），软件不用再手写异或——趋势是"把 swizzle 下沉进硬件"。

## 动手绑定

| 做什么 | 位置 |
|--------|------|
| 读最小示例 | `swizzle/mma_simple_swizzle.cu`（异或函数怎么写、怎么用） |
| 读可视化 | `swizzle/print_swizzle_layout.py` 跑一下，亲眼看 XOR 模式 |
| 读工程落地 | `hgemm/mma/swizzle/hgemm_mma_stage_swizzle.cu`（对照 basic 版，diff 出 swizzle 增量） |
| 转置场景 | `swizzle/mat_trans_swizzle.cu` + `mat-transpose/mat_transpose_cute.cu` |

## 记忆钩子

"ldmatrix 大口喂，swizzle 错位防撞车；写乱读同乱，逻辑不改一点。"

**人话版**：**ldmatrix** = 一条指令从 shared memory 大口加载一小块矩阵、直接装进 Tensor Core 要用的
寄存器组（frag）——比逐元素读取快一个量级。但它的加载方式容易撞 bank（第 05 题），加 padding 又
会破坏它的对齐要求——**swizzle** 解决：写入 shared 时按固定的异或规律把位置**故意放乱**，读取时
按**同样的规律**还原，谁也不撞谁，而且逻辑上数据顺序完全没变（乱在物理位置，不乱在逻辑）。
