# 25 Profiling 流程：nsys 定位 → ncu 深挖

## 面试口述版

两把刀各管一层，先粗后细：
**nsys（Nsight Systems，时间线层）**：看**全局**——kernel 顺序、gap、memcpy、多流并发是否真的并发、
CPU 侧 launch 延迟。它回答"时间花在哪一段/有没有空隙/重叠有没有成立"。
典型用法 `nsys profile -o out ./app && nsys-ui out.nsys-rep`（或 `nsys stats` 出表）。
**ncu（Nsight Compute，单 kernel 层）**：钻进**一个 kernel**——SM/显存利用率、occupancy、
warp state（stall 原因）、roofline 位置、指令级热点。它回答"这一个 kernel 为什么慢"。
典型用法 `ncu --set full -k 正则 ./app`；先 `--set basic` 快扫再定点深挖（full 很慢）。
**标准工作流**：nsys 找到最耗时/有空隙的 kernel → ncu 单独剖析它 → 判断访存/计算受限（roofline，联动 19）
→ 对症改 → 两把刀复测对比。反模式是"上来就 ncu 全量采集"，慢且抓不到空隙与并发问题。
面试演示路径（你的机器全配好了）：用 nsys 看 notes-v2 的多 kernel 时间线，用 ncu 剖 HGEMM MMA 的
SM% / Memory%，两个数字一对比 roofline 就知道下一步该优化什么。

## 高频追问

- **ncu 指标里最该先看哪几个？** SOL（Speed Of Light）页的 SM % 和 Memory %（先定位受限类型）→ Occupancy 的限制因素 → Warp State 的 top stall（如 long scoreboard = 等显存）→ Memory Workload 的事务放大/bank conflict。
- **ncu 会不会影响被测性能？** 会（串行化 replay kernel），所以 ncu 的绝对时间不可信，**比例和计数器才可信**；绝对时间用 event/nsys。
- **远程服务器没有图形界面怎么办？** 全部 CLI：`nsys stats` 出文本表、`ncu --page details --csv` 导出分析——无头服务器是你的日常环境，别只会点 GUI。
- **和分布式训练排查的关系？** 训练 hang/慢先上 nsys 时间线看 collective 是否咬合（等哪个 rank），这是你海光多机实验里每天都在用的肌肉，面试把它讲成方法论。

## 动手绑定

| 做什么 | 命令（工具已装在 /usr/local/cuda-12.8/bin） |
|--------|------|
| 时间线层 | `nsys profile -o /tmp/nv2 --force-overwrite true ./notes_v2_cute_sm89.bin --bench-hgemm`，再 `nsys stats /tmp/nv2.nsys-rep` |
| kernel 层 | `ncu --set basic --launch-count 2 ./notes_v2_cute_sm89.bin --bench-hgemm`，看 SOL 两率 |
| roofline | `ncu --set roofline ...` 直接出图 |
| 读现成笔记 | `nvidia-nsight/README.md` + `bank_conflicts.md`（LeetCUDA 的 profiling 专题） |

**WSL 实测备注（2026-08 在本机验证）**：`nsys` 直接可用；`ncu` 报 `ERR_NVGPUCTRPERM`——
WSL 下 GPU 性能计数器默认关闭，需在 **Windows 侧 NVIDIA 控制面板 → 开发人员 → 管理 GPU 性能计数器权限 →
允许所有用户访问**，重启 WSL 后生效（实验室 Linux 服务器无此问题）。见不到 ncu 之前，
先用 nsys + `--bench-all` 的 TFLOPS 数字做分析，一样能讲清定位逻辑。

## 记忆钩子

"nsys 看全局找空隙，ncu 钻单 kernel 找病根；SOL 定受限，stall 找原因；比例可信，绝对时间别信 ncu。"

**人话版**：两把刀分工——**nsys**（系统级时间线）：看整个程序里 kernel 的先后、空隙、多流有没有
真的并发；**ncu**（单 kernel 显微镜）：钻进一个 kernel 看它为什么慢。ncu 里先看 **SOL** 页
（Speed Of Light，"当前性能 ÷ 理论极限"两条百分比：SM % 和 Memory %——谁高就是谁受限），
再看 **stall 原因**（warp 在等什么：等显存？等寄存器？）。最后一条铁律：ncu 为了采集会反复重放
kernel，它给出的**绝对时间不可信**，但比例和计数器可信；绝对时间用 event 和 nsys。
