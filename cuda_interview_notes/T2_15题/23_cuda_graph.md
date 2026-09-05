# 23 CUDA Graph：捕获整图一次发射

## 面试口述版

动机：**launch 开销与 CPU 瓶颈**。一个推理 step 由几十上百个小 kernel 组成时，
每个 launch 的 CPU 侧开销（~5-10μs）累积起来远超 GPU 执行时间，GPU 在"等投喂"。
**CUDA Graph** 把一整段 DAG（kernel/memcpy/event 依赖关系）**一次性捕获成图，
之后每次只需一次 launch**（cudaGraphLaunch），CPU 开销从 O(N) 降到 O(1)，
且驱动能做全图优化（依赖明确、调度更紧）。
两种创建方式：**stream capture**（`cudaStreamBeginCapture` → 像平常一样往流里塞操作 →
`EndCapture` 得到图，代码零改动）和显式 API（cudaGraphAddKernelNode 逐节点建，控制精确但啰嗦）。
参数更新用 `cudaGraphExecKernelNodeSetParams` 原地改，不必重建图。
适用边界：结构**固定**的工作负载（推理 step、训练一个迭代内的固定子图）收益大；
每次形状/依赖都变的动态负载收益小甚至为负（捕获本身有成本）。

## 高频追问

- **为什么推理引擎都爱它？** vLLM/SGLang 的 decode 路径小 kernel 密集（attention + GEMM + 采样），capture 成 graph 后 CPU 解放、step 时间显著下降；配合 batch 固定形状正合适。
- **动态 shape 怎么办？** 按 shape 桶（bucket）预 capture 多张图，运行时选最接近的；或 setParams 原地改指针/尺寸。
- **和 stream 的关系？** Graph 是"录下来的流操作 + 依赖"，回放时仍按依赖执行；capture 期间不能有同步类操作（event wait 除外）。
- **通信场景能用吗？** NCCL collective 可被 capture（NCCL 2.14+），推理侧的通信计算重叠图也是 graph 的用武之地——和你课题直接相关。

## 动手绑定

LeetCUDA 无 Graph demo（单 kernel 场景确实用不上）——这题按**概念题**准备，以下三件事作为功课：

1. 读官方概念页 + vLLM 的 graph capture 代码（`vllm/worker/gpu_model_runner.py` 搜 `capture`），看工业用法；
2. 用 demos/stream_overlap.cu 改造：把"双流四步"用 stream capture 包成 graph，回放对比 CPU 开销（可选练习）；
3. 口述题自测："capture 的限制有哪些？"（不能同步、不能动态分支、操作集合固定）。

## 记忆钩子

"小 kernel 太多 CPU 喂不动；录一遍 DAG，一键回放；形状固定才划算。"

**人话版**：推理一步要发射上百个小 kernel，每次发射 CPU 都要花几微秒，GPU 大量时间在**等 CPU
投喂**。**CUDA Graph** = 把这一整串任务（含依赖关系）**录制成一张图**，之后每次执行只需一次调用
回放整张图——CPU 开销从上百次降到一次。前提：计算结构要**固定**（形状不变、依赖不变）才划算；
每次都变的动态负载录图反而亏。
