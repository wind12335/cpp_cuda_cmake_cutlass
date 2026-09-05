# 22 Kernel 里能用什么、不能用什么

## 面试口述版

device 代码是一个**无系统调用、无标准库全量支持**的执行环境，红线清单：
**不能用**：抛异常/try-catch（device 侧无展开机制）；动态分配用 new/malloc 勉强可用但极慢且无回收语义（别用）；
标准库容器/iostream；递归要谨慎（调用栈显存模拟，深度和速度都差，Fermi 后支持但基本是反模式）；
host 指针直接解引用（统一寻址没开就是非法地址）。
**能用但要懂代价**：`printf`（有缓冲、顺序不保证、严重拖慢——只用于调试）；
`assert`（编译期 NDEBUG 控制）；`atomicAdd` 等（吞吐远低于普通运算，高竞争要分桶——联动 C++ 22 题）；
`__syncthreads()`（**必须全体线程可达**，放在 divergent 分支里直接死锁——高频考点）；
`__shared__` 静态/动态（动态版 launch 第三参数）。
**完全禁止**：kernel 里再 launch 别的卡上的东西、访问未配套的 host 资源。
反向考点：`__syncthreads()` 放在 `if (threadIdx.x == 0)` 里会发生什么——其余线程永远到不了屏障，
block 死锁挂死，训练里"莫名 hang"的一大来源（和 NCCL 的 collective 不齐 hang 是同款逻辑）。

## 高频追问

- **怎么在 device 里做动态数据结构？** 预分配 + 索引池（arena/自由链表），或干脆在 host 侧组织好传进来——"device 端无分配器思维"。
- **递归 alternatives？** 显式栈数组 / 迭代化；真要递归的算法（树遍历）常见做法是数据反向布局成数组层序。
- **unified memory 会不会让红线消失？** 不会：UM 只解决指针可访问性，执行环境的限制（无异常、慢分配）依旧。

## 动手绑定

| 做什么 | 位置 |
|--------|------|
| 读"干净 kernel 长什么样" | `elementwise/relu.cu`（nvidia-nsight 目录版最精简）与 `relu/relu.cu`（torch 封装版） |
| 读 __syncthreads 的正确位置 | `interview/notes-v2.cu` Phase 1 Block Reduce（屏障只在全 block 路径上） |
| 亲手造死锁（可跑的反例） | 把 Block Reduce 的 `__syncthreads()` 移进 `if (threadIdx.x < 16)` 里，跑挂后 Ctrl-C——比读十遍印象深刻 |

## 记忆钩子

"device 无异常无 STL，printf 是止血钳；__syncthreads 必须全票通过，分歧里放屏障 = 集体挂死。"

**人话版**：kernel 里没有操作系统兜底：不能抛异常、不能用标准库容器、动态分配又慢又危险。
`printf` 能用但严重拖慢，只当调试的"止血钳"。最重要的红线是 **`__syncthreads()`**（block 内
所有线程的集合屏障）：必须 block 里**每一个线程都执行到它**才有效——如果把它放在"只有部分线程
会进入"的分支里，另一部分线程永远到不了，全 block 集体挂死。
