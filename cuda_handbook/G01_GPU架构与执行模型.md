# G01 GPU 架构与执行模型 【概念，写代码前的世界观】

## §G01.1 异构计算总图

**作用**：理解"CPU 和 GPU 是两个独立设备，各有各的内存"——这是 CUDA 一切 API 的由来。

```
   Host (CPU + 主机内存)                    Device (GPU + 显存)
  ┌─────────────────┐                    ┌─────────────────┐
  │ C++ 代码         │   PCIe/NVLink      │ 几千个 CUDA 核心  │
  │ cudaMalloc 分配  │ ────搬运数据────→  │ kernel 并行执行   │
  │ cuda* 系列 API   │ ←──(慢!瓶颈!)────  │ 结果写回显存      │
  └─────────────────┘                    └─────────────────┘
```

数据流永远是：host 准备数据 → 搬到 device → 启动 kernel 计算 → 搬回 host。
**搬数据（PCIe ~16-64GB/s）比计算慢得多**——"少搬数据、能留在显存就别搬"是第一戒律
（量级表见 os_net NET05）。

## §G01.2 线程层级：Grid → Block → Warp → Thread

**作用**：CUDA 用"两层分组"组织海量线程，两层各管一件事。

```
Grid（一次 kernel 启动的全部线程）
 ├── Block(0,0)  ├── Block(1,0)  ├── Block(2,0) ...   ← Block 之间: 独立, 不能通信
 │   ├── Warp0 (线程 0~31)                             ← Warp: 32 线程一组, 调度单位
 │   │   ├── thread(0,0) thread(1,0) ...               ← Thread: 最小执行单位
 │   ├── Warp1 ...
 │   └── shared memory (Block 内共享的一块片上内存)
```

| 层级 | 索引变量 | 规则 |
|------|---------|------|
| Thread | `threadIdx.x/y/z` | block 内编号 |
| Block | `blockIdx.x/y/z` | grid 内编号 |
| 每层大小 | `blockDim / gridDim` | 启动时指定，kernel 里只读 |

两条铁律：**一个 block 只能待在一个 SM 上**（所以 block 内线程能用 shared memory 通信）；
**warp=32 线程锁步执行**（分支/访存模式都按 32 粒度考虑）。
【前置：面试 T1-01、cuda_interview_notes G01 有更细的机制讲解】

## §G01.3 你这张卡的参数（RTX 4060 Laptop）

| 参数 | 值 | 意义 |
|------|----|----|
| 架构/编号 | Ada Lovelace / **sm_89** | 编译 flag `-arch=sm_89` |
| SM 数 | 24 | block 数要按这个倍数铺 |
| 每 SM 最大线程 | 1536（=48 warp） | occupancy 的分母 |
| 每 SM shared memory | 100KB（可配置，单 block 默认上限 48KB） | tile 大小的天花板 |
| 寄存器堆 | 每 SM 256KB（每线程最多 255 个） | 寄存器压力来源 |
| L2 缓存 | 32MB | 全卡共享 |
| 显存 | 8GB GDDR6, ~272GB/s | 全部数据的家 |
| 计算/显存峰值 | ~15 TFLOPS FP32 / 272 GB/s | roofline 拐点 ≈ 55 FLOP/B |

查询代码：`cudaGetDeviceProperties`（G02 lab 里有）。

## §G01.4 执行模型：靠"多线程换班"藏延迟

**作用**：理解 GPU 为什么慢的线程也能快——因为多。

- CPU：线程少，靠缓存+乱序执行**降低单线程延迟**；
- GPU：线程几千个，某个 warp 等显存（几百 cycle）时，调度器**立刻切换**到别的 warp 执行——
  只要"够多的可运行 warp"，等待时间就被别的计算填满。

推论：** occupancy（驻留 warp 数）是藏延迟的资源**（前置：面试 G07），而分支、小 tile 都会
减少"可运行的 warp"，伤害性能。

## §G01.5 一次 kernel 执行的完整生命周期

```
1. host: cudaMalloc      → 显存里分配输入/输出缓冲
2. host: cudaMemcpy(H2D) → 数据搬到显存
3. host: kernel<<<g,b>>>() → CPU 发一个"启动命令"给 GPU (异步! 立即返回)
4. GPU:  几千线程并行执行
5. host: cudaMemcpy(D2H) → 结果搬回来 (会隐式等待 kernel 完成)
6. host: cudaFree        → 释放
   全程错误检查: cudaGetLastError / cudaDeviceSynchronize (G02)
```

【前置：C09 并发】第 3 步"异步立即返回"意味着 **CPU 计时不准**——要用 event（G06）。

## 自测题（能答出才算读懂本章）

1. 为什么"两个 block 之间不能通过 shared memory 通信"？
2. 为什么 kernel 里 `if (threadIdx.x == 0) 走慢路径` 比 `if (blockIdx.x == 0)` 伤害大？
3. 你的卡上，一个 block 最多多少线程？grid 最大多少个 block？
（答：①block 被绑死在一个 SM，SM 换不了家；②threadIdx.x 分歧发生在 warp 内；③1024；grid 各维上限 2³¹-1/65535/65535）
