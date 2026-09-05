# 01 GPU 执行模型：SM / warp / SIMT

## 面试口述版

GPU 是**吞吐机**不是延迟机：几万个小线程排队吃满算力，单个线程慢无所谓，整体带宽打满就行。
层级上：**SM（流式多处理器）是物理执行单元**（4060 Laptop 有 24 个 SM，H100 上百个），
每个 SM 里有调度器、寄存器堆（~256KB/SM）、shared memory/L1（~128KB/SM，可配置划分）。
线程以 **warp=32 线程**为最小调度单位：同一 warp 的 32 个线程**锁步执行同一条指令**（SIMT），
遇到分支就分叉串行（见 06 题）。一个 block 只会驻留在**一个** SM 上（block 内线程能通过 shared memory 通信的原因），
一个 SM 可同时驻留多个 block/warp——某个 warp 等显存时调度器切到别的 warp，**用并行藏延迟**（CPU 用缓存/乱序藏延迟，思路完全不同）。

## 高频追问

- **CPU 和 GPU 编程模型的本质区别？** CPU 少核强单线程、低延迟优先（大缓存、乱序、分支预测）；GPU 多核弱单线程、吞吐优先（小缓存、靠 warp 切换藏访存延迟）。所以优化目标不同：CPU 看延迟，GPU 看占用率×带宽利用率。
- **为什么 warp 是 32？** 硬件设计选择（一个调度器一拍发射 32 路 lanes）；写代码要记住的是"分支/访存模式都以 32 为粒度"。
- **block 越大越好吗？** 不是——block 大占寄存器/shared 多，SM 驻留的 block 数变少，反而藏不住延迟（联动 07 occupancy）。
- **和 NCCL 的关系？** NCCL 的 ring kernel 也是"少量线程啃满带宽"的吞吐思路，和你 overlap 论文里"计算 kernel 与通信 kernel 在不同流上并发"同理。

## 动手绑定

| 做什么 | 命令 |
|--------|------|
| 读"面试框架速查"总纲 | `LeetCUDA/kernels/interview/notes-v2.cu` 开头 Phase 0 注释（L11-30） |
| 跑通全部 kernel 验证表（直观感受"几十个 kernel 一次全对"） | `cd ~/workspace/LeetCUDA/kernels/interview && ./notes_v2_cute_sm89.bin` |
| 查自己 GPU 的 SM 数/参数 | `nvidia-smi -q \| grep -i "sm\|multiproc"` 或 deviceQuery |

## 记忆钩子

"SM 是车间，warp 是 32 人一班锁步干；block 进一个车间，靠换班藏等料的空闲。"

**人话版**：四个词逐个翻译——**SM**（流式多处理器）= GPU 上的物理车间（4060 有 24 个）；
**warp** = 32 个线程编成一个班，全班**执行完全相同的指令**（锁步，SIMT）；
**block** = 一支施工队，整队只能进**一个**车间（所以队友之间能用 shared memory 传料）；
**换班藏空闲** = 一个车间里同时驻留很多班，某班在等显存数据时，调度器立刻切到别的班干活——
GPU 靠"人多好换班"来掩盖访存延迟，而不是像 CPU 靠大缓存。
