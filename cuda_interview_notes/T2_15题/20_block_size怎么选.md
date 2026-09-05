# 20 Block Size / Launch 配置怎么选

## 面试口述版

没有万能配置，但有清晰的推理框架，三问定配置：
① **warp 对齐**：block size 取 32 的倍数，常见 **128 或 256**——太小（<64）调度开销占比大，太大（>1024 上限）直接非法；
② **问题形态**：
- elementwise/规约类：每线程工作量小、靠并行藏延迟 → 256 线程 + 足量 block 铺满 SM；
- GEMV/带宽型大循环：warp-per-row + 每线程 128 元素大循环（hgemv 的设计），block 数不多但每线程吃带宽；
- GEMM/MMA：block 形状由 tile 决定（128×128 tile 配 8 warp ≈ 256 线程），size 是结果的体现不是输入；
③ **资源天花板**：shared 用量、寄存器用量反推能驻留几个 block（联动 07），必要时 `cudaOccupancyMaxPotentialBlockSize` 让运行时建议。
另外记住：**shared 大小是 launch 的第三个参数**（动态 shared），要和 kernel 内 `extern __shared__` 配套。

## 高频追问

- **block 内线程间要通信/同步，怎么影响选择？** 需要协作的（如规约、halo 交换）倾向大 block（128-512）减少跨 block 同步；独立任务小 block 即可。
- **为什么 block 数略多于 SM 数×驻留数好？** 让"算完的 block 立刻有下一个补位"（尾部效应小），LeetCUDA 里 grid 取 ceil + 多 SM 倍数就是这个考虑。
- **不同 GPU 代际要改配置吗？** SM 数/warp 槽位/容量都在变（4060 24 SM vs H100 132 SM），生产 kernel 常按 deviceQuery 结果自适应 grid——"写死 grid size"是移植性 bug。

## 动手绑定

| 做什么 | 位置 |
|--------|------|
| 对照三种哲学的真实配置 | `elementwise/elementwise.cu`（256 线程铺满型）/ `hgemv/hgemv.cu`（warp-per-row 大循环型）/ `hgemm/mma/basic/hgemm_mma.cu`（tile 决定型），各看 launch 那一行 |
| 亲手改着玩 | 把 elementwise 的 block 从 256 改 64/512，`--bench-all` 看吞吐变化，自己解释 |
| 查运行时建议 | 写 5 行代码调 `cudaOccupancyMaxPotentialBlockSize`（或读 notes-v2 里相关用法） |

## 记忆钩子

"先问要什么形态：铺满、吃带宽、还是喂 Tensor Core；size 是推理结果，不是玄学常数。"

**人话版**：block 大小没有万能值，按任务形态推——**铺满型**（elementwise 类）：256 线程、
尽量多的 block，把 SM 塞满打带宽；**吃带宽型**（GEMV 类）：每线程带大循环，block 数不多但单个
线程干活猛；**喂 Tensor Core 型**（GEMM）：block 大小由 tile 形状推导出来（128×128 的 tile 配
8 个 warp = 256 线程），是设计的结果而不是拍脑袋的输入。共同底线：取 32 的倍数、不超过 1024。
