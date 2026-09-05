# 12 Double Buffering / cp.async / 多级流水

## 面试口述版

Tiling 之后的新瓶颈是**流水线气泡**：每个 K 迭代里"从 global 搬下一个 tile（慢）→ 等搬完 →
再算当前 tile（快）"，搬运期间计算单元全在等——数据搬运和计算**串行**了。
**Double buffering**：shared memory 开**两块**缓冲，一块在"算"，另一块同时在"搬"下一轮的数据；
每轮迭代末尾交换两块的角色——搬运延迟被计算时间藏住，SM 不再空转。
现代等价物是 **cp.async**（Ampere+）：绕过寄存器直接 global→shared 的异步拷贝，
配 `commit_group`/`wait_group<N>` 精确控制"等哪几批到货"，多级流水（multi-stage，2~4 级）就是它的规模化。
再往后是寄存器级 double buffering（swizzle 版里每线程再开双份寄存器）和 Hopper 的 TMA+WGMMA 流水。
这套"预取-重叠"思想你无比熟悉：**和你论文里把通信藏进计算是同一个模型**——通信 stream 是"搬 tile"，
计算 stream 是"算 tile"，event 对应 wait_group。

## 高频追问

- **stages 越多越好吗？** 不是：每多一级就多一份 shared 开销，挤占 occupancy（联动 07）；LeetCUDA hgemm 的 kStages 模板参数就是让你实测这个 trade-off。
- **cp.async 比"先读寄存器再写 shared"好在哪？** 省一半寄存器流量、指令更少、且是真正的异步发射；老一代（Pascal-）只能靠"提前一轮手读"模拟。
- **wait_group(1) 和 wait_group(0) 什么区别？** 等"只剩 ≤1 组未完成"vs"全部到货"——流水线深度由这个数字决定。

## 动手绑定

| 做什么 | 位置 |
|--------|------|
| 读三代演进 | `sgemm/sgemm_async.cu`（FP32 手动 prefetch）→ `hgemm/naive/hgemm_async.cu`（cp.async）→ `hgemm/mma/basic/hgemm_mma_stage.cu`（kStages 多级流水 + MMA） |
| 读极限版 | `hgemm/mma/swizzle/hgemm_mma_stage_swizzle.cu`（多级流水 + 寄存器 double buffer + swizzle 三件套） |
| 实测 | `--bench-hgemm` 里逐个变体的 TFLOPS 对比：stage 数从 2→3 的增益自己看 |

## 记忆钩子

"一块算来一块搬，轮流坐庄不断档；cp.async 发货不签收，wait_group 控在途单数。"

**人话版**：**double buffering** = shared memory 开两块缓冲：一块正在被计算，另一块同时在接受
下一轮的数据，每轮结束两块换角色——计算等数据的空档消失了。
**cp.async** 是新式异步搬运指令："发货不签收" = 发出拷贝命令后不等待完成，继续干别的；
**wait_group** 才是签收点："等到在途的拷贝组只剩 N 组"——N 越大流水线越深，但占用的缓冲也越多。
