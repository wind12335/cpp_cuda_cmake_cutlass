# 18 FlashAttention 为什么快

## 面试口述版

标准 attention 的死穴是**中间矩阵要完整读写显存**：S=QKᵀ 是 N×N，softmax 后的 P 也是 N×N，
显存读写量 O(N²)，N 大时带宽直接锁死、显存直接爆。
**FlashAttention 的两板斧**：
① **Tiling**：Q/K/V 分块进 shared，S 和 P 只在片上生、片上死，显存读写降为 O(N²/M)（M 是 tile 大小），
显存占用从 O(N²) 降到 O(N)；
② **Online Softmax**：softmax 的分母需要"整行最大值与整行和"，但分块时只能看到部分 K——
解法是携带**运行统计量**（running max m 和 running sum l）逐块重缩放：新块的 max 更大就把
之前的累加结果按 `exp(m_old - m_new)` 缩放修正，数学上与全局 softmax 等价。
这就是"online softmax"——它和你的 reduce 预累加是同族思想（部分结果 + 修正系数）。
进阶：FA-2 调整循环顺序（外层 K 内层 Q，减少 HBM 写）、split_kv 并行（LeetCUDA 的 split_kv/split_q 变体）、
FA-3 用 WGMMA/ping-pong。**面试最常追问的就是 online softmax 的重缩放推导**——要能当场推。

## 高频追问

- **重缩放公式当场推一遍？** 新块局部 max m_new > 旧 m_old 时，旧累计 l ← l·exp(m_old−m_new)，旧输出 O ← O·exp(m_old−m_new)，再加新块的 exp(s−m_new)·V——两行讲清。
- **为什么后向更省？** 前向只存 m/l 和 O，反向重算注意力而非存 N² 矩阵——"重算换显存"。
- **和你的 overlap 课题？** decode 阶段 attention 是访存受限的"小计算"，正好留出 SM 余量去藏通信——推理侧 overlap 方案（如双 micro-batch）就是靠这种算力空隙，面试能自然衔接。

## 动手绑定

| 做什么 | 位置 |
|--------|------|
| 读 FA-2 主实现（含 online softmax） | `interview/notes-v2.cu` Phase 8（FA-2 split_q，注释详细） |
| 读变体家族 | `flash-attn/mma/basic/`：`split_kv` vs `split_q` vs `share_kv/share_qkv`（shared 复用策略对比） |
| 读 swizzle 版 | `flash-attn/mma/swizzle/`（q/qk/qkv 三种 swizzle 粒度） |
| 跑验证 | `./notes_v2_cute_sm89.bin`（MergeAttnStates / FA 相关行）与 `--bench-fa` |

## 记忆钩子

"N² 中间不落地，片上生片上死；max 变了别慌，旧账乘个 exp 修正——online softmax。"

**人话版**：普通 attention 要把 N×N 的中间矩阵完整写进显存再读出来，N 一大显存和带宽双双爆炸。
**FlashAttention** 分块计算，中间结果只在片上（shared/寄存器）出生和消亡，永远不整体落地显存。
难点是 softmax 需要"整行的最大值"做分母——分块时看不到整行怎么办？**online softmax**：随身携带
"到目前为止的最大值和总和"，新块出现更大的 max 时，把之前的累加结果**乘一个修正系数**
exp(旧max − 新max) 再继续——数学上和一次算完完全等价。
