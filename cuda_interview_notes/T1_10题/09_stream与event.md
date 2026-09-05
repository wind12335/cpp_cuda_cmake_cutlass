# 09 Stream + Event：异步、重叠与计时

## 面试口述版

**Stream 是 GPU 上的任务队列**：同一 stream 内操作按序执行，**不同 stream 之间无序、可并发**——
这就是计算/通信重叠的硬件基础。默认流（legacy default stream）有隐式同步语义，实际工程都用显式非默认流。
**Event 是队列里的标记**：`cudaEventRecord` 在流里打个点，`cudaEventSynchronize/ElapsedTime`
能测时间，`cudaStreamWaitEvent` 让另一个流等这个点——跨流的依赖关系全靠它表达。
**重叠的正确姿势**：把独立任务分到不同流（比如分块 H2D → 计算 → D2H 流水线：流 A 在算第 i 块时，
流 B 在传第 i+1 块）；依赖用 event 串起来；最后 `cudaStreamSynchronize` 收口。
你的 overlap 论文就是这个模型的宏观放大版：NCCL 通信 kernel 在通信流上跑，
计算 kernel 在计算流上跑，靠 event 控制数据依赖——面试时这是你把"基础 API"讲成"系统能力"的最佳衔接点。

## 高频追问

- **非默认流之间什么会破坏并发？** 资源真冲突（显存带宽饱和、SM 被占满时并发只是排队）+ 隐式同步（legacy default stream 和所有流互斥）+ CPU 端忘了异步就 `cudaMemcpy` 同步了。
- **怎么给 kernel 指定流？** launch 的第 4 个参数 `kernel<<<grid, block, smem, stream>>>`；memcpy 用 `cudaMemcpyAsync(..., stream)`。
- **event 计时为什么比 CPU 计时准？** 事件在 GPU 时间线上打点，测的是 GPU 侧真实耗时，不受 CPU 调度抖动影响；记得用第一个 event 同步过再开始记。
- **多 stream 一定更快吗？** 单任务塞不满 GPU 时才有意义；塞满一个流的任务，多流只是排队换了张票。

## 动手绑定

配套 demo `demos/stream_overlap.cu`（LeetCUDA 无此主题，这属于你论文的看家本领，必须能白板写）：

```bash
nvcc -std=c++17 -O2 -arch=sm_89 demos/stream_overlap.cu -o /tmp/so && /tmp/so
```

输出包含：单流串行 vs 双流重叠的耗时对比 + event 计时，再配 pinned（08 题）效果更明显。
读代码：`interview/notes-v2.cu` 的 bench 部分（`--bench-hgemm` 等入口）用 cudaEvent 计时，可对照学事件用法。

## 记忆钩子

"流是队列，队列间自由；event 是路标，跨队靠它让；重叠 = 独立任务进不同队。"

**人话版**：**stream（流）**= 一个按顺序执行的任务队列；同一个流里排队，**不同流之间**没有先后
约束、可以并发。**event（事件）**= 插在流里的一个路标：用来计时（两个路标一夹）、或者让另一个流
"走到这个路标再继续"（跨流依赖）。**重叠**的本质 = 把互不依赖的任务塞进不同流，让"传数据"和
"算数据"同时进行——这正是你 overlap 论文在系统级的微观原型。
