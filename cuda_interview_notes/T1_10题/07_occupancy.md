# 07 Occupancy（占用率）

## 面试口述版

Occupancy = **SM 上实际驻留的 warp 数 ÷ 理论最大 warp 数**（4060 单 SM 最多 48 个 warp 量级）。
它衡量的是"SM 里有多少可切换的候补"，occupancy 高 = 某个 warp 卡在显存等待时，调度器有别的 warp 可切，
**延迟被并行藏住**。三个限制因素，哪个先顶到天花板哪个说了算：
① **寄存器**：每线程寄存器多（MMA 大 tile 常见），能驻留的 warp 就少；
② **shared memory**：每 block 用得多（double buffering 的两块大 tile），驻留 block 数下降；
③ **block 槽位**：单 SM 能驻留的 block 数有硬上限（~16-32）。
**关键观念：occupancy 不是越高越好**——它是"藏延迟的手段"不是目的。
compute-bound 的大 GEMM 只要 25-50% occupancy 就喂饱 Tensor Core 了，盲目冲高 occupancy
（比如减小 tile）反而伤性能；memory-bound 的 elementwise 才需要高 occupancy 打满带宽。
ncu 的 Occupancy 页会告诉你"理论 occupancy、实际 occupancy、以及是哪个因素限制的"。

## 高频追问

- **怎么主动调 occupancy？** launch bounds（`__launch_bounds__(maxThreads)`）或编译选项 `-maxrregcount` 压寄存器；调 shared 卡大小（cudaFuncSetAttribute）；调 block 大小。
- **低 occupancy 但高性能的例子？** hgemv 的 warp-per-row 设计：每线程 128 元素大循环吃满带宽，occupancy 低照样打满——判断标准是带宽/算力利用率，不是占用率本身。
- **MMA kernel occupancy 低正常吗？** 正常：寄存器大 tile 是拿来喂 Tensor Core 的，sub-50% 很常见（你跑 hgemm bench 时 ncu 一看便知）。

## 动手绑定

| 做什么 | 位置 |
|--------|------|
| 实测对照（同一题，两种占用哲学） | `hgemv/hgemv.cu`（warp-per-row：低占用高吞吐）vs `elementwise/elementwise.cu`（高占用打满带宽） |
| 读 notes-v2 Phase 0 的优化清单 | `interview/notes-v2.cu` 开头注释（occupancy 在优化 checklist 的位置） |
| 用 ncu 看 | `ncu --set full ./notes_v2_cute_sm89.bin --bench-hgemm` 看 Occupancy 页的限制因素（联动 25 题） |

## 记忆钩子

"占用率是候补人数，不是 KPI；卡在哪个资源天花板，ncu 一页看明白。"

**人话版**：occupancy = SM 里实际驻留的 warp 数 ÷ 理论最大 warp 数。它高的意义是：某个 warp 卡在
等显存时，SM 还有别的 warp 可以立刻顶上干活——所以它是**藏延迟的手段**，本身不是性能指标
（KPI）。只算一个乘法的 kernel 把 occupancy 冲到 100% 也没用；大 GEMM 的 Tensor Core 版本
occupancy 常常只有三四十但跑得飞快。被什么卡住（寄存器/shared/槽位上限），ncu 的 Occupancy 页
直接写明。
