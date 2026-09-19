# G08b GPU 三件套实战：nvitop / nsys / ncu

**【前置标注】** G08（工具总览）；C09b（gdb——同样的"透视镜"思想）。

**【记忆钩子】** **nvitop 看"现在"，nsys 看"全程"，ncu 钻"单个 kernel"**——像医院三件套：
体温计（当前状态）、全身体检时间线（谁占时间）、单器官 CT（细胞级指标）。

**【人话版】** 程序慢了，先拿体温计量一下（GPU 是不是根本没忙起来？），再做全身体检
（时间都花在哪个阶段？），最后对可疑器官做 CT（那个 kernel 到底卡在带宽还是占用率）。
**三件套是递进关系，不是三选一。**

---

## 一、分工总表（先记住这张）

| 工具 | 回答的问题 | 粒度 | 类比 C09 |
|---|---|---|---|
| `nvitop` | GPU **现在**什么状态？谁在用？ | 秒级实时 | htop 之于 CPU |
| `nsys` | 程序**全程**时间花在哪？拷贝/计算重叠了吗？ | 毫秒级时间线 | nsys 的时间线 ≈ 多线程交错图 |
| `ncu` | **这一个 kernel** 的瓶颈是什么？ | 单 kernel 指标 | ncu 的指标 ≈ 寄存器面板（透视内脏）|

## 二、nvitop：GPU 的 htop（已装 1.7.1，uv tool 独立环境）

```bash
nvitop          # TUI 全屏面板(敲 q 退出)
nvitop -1       # 单帧打印(脚本里用)
```

**看什么**（本机实测一帧）：温度 50C / 功耗 15W / 显存 856MiB÷8188MiB / GPU-Util 10% /
**进程表（哪个 PID 在占 GPU、占多少显存）**——比 `nvidia-smi` 强在：彩色阈值、按进程排序、
可交互（选中进程 kill、改刷新率）、CPU/内存条同屏。

**用法时机**：跑大实验前先开一扇 `nvitop`（tmux 分屏最佳）——kernel 跑挂了、显存泄漏了、
别人占卡了，**一眼看见**。WSL 下进程名偶尔显示 No Such Process（Windows 侧进程的显示限制），
显存/利用率数字不受影响。

## 三、nsys：全程时间线（"程序慢在哪一段"）

```bash
nsys profile -t cuda -o 报告名 ./程序        # 采集(生成 .nsys-rep)
nsys stats 报告名.nsys-rep                   # 命令行看统计报表
nsys stats --report cuda_api_sum 报告名      # 指定报表(API 耗时汇总)
nsys-ui 报告名.nsys-rep                      # 图形界面看时间线(WSL 里配 X server/WSLg)
```

**本机实测例子**（小白鼠 kernel 的 API 汇总）：

```
Time(%)  Total(ns)   Name
  99.1   175283658   cudaMalloc      ← 175ms! 大头根本不是 kernel
   0.4      700328   cudaMemset
   0.4      624287   cudaLaunchKernel
```

**这就是 nsys 的价值：定位"慢的到底是哪一段"**——上面这份报告告诉你该优化的是**内存分配**
（先分配后复用！），而不是 kernel。G06 的流重叠实验也靠它看时间线：拷贝和 kernel 的条形
**有没有叠着**，一眼判断 overlap 成没成。

**常用报表**：`cuda_api_sum`（API 耗时）、`cuda_gpu_kern_sum`（各 kernel 总耗时）、
`cuda_gpu_mem_time_sum`（拷贝耗时）。

## 四、ncu：单 kernel CT（"瓶颈是带宽还是占用率"）

### 4.1 基本用法（全部本机实测）

```bash
ncu --set basic ./程序                     # 剖析程序里所有 kernel(基础指标集)
ncu --set basic -k relu_f16x2 ./程序       # 只剖析指定名字的 kernel
ncu --set basic -k "regex:relu" -c 1 ./程序  # regex 匹配, 每个只采 1 次
ncu --set full -o /tmp/报告 ./程序          # 全量指标落盘(肥! 用完即删)
```

**编译时记得带行号信息**（否则只能看汇编级，看不到源码行）：

```bash
nvcc -arch=sm_89 -O3 -g --generate-line-info x.cu -o x.bin
```

### 4.2 核心指标怎么读（四列定生死）

| 指标 | 含义 | 怎么用 |
|---|---|---|
| **Memory Throughput %** | 带宽用了几成 | 高(>80%) = 访存受限，优化方向是**少搬数据** |
| **Compute (SM) Throughput %** | 算力用了几成 | 高 = 计算受限，优化方向是**少算/用 tensor core** |
| **Achieved Occupancy %** | 实际在场 warp 占比 | 低 = "工位没坐满"，常见原因：寄存器太多/块太小 |
| **Duration µs** | 单次耗时 | 优化的最终裁判 |

### 4.3 首课案例：relu 五连对比（本机实测数据）

| kernel | 带宽 | 耗时 | 算力 | 占用率 | 解读 |
|---|---|---|---|---|---|
| f16 朴素 | 60.0% | 497.6µs | 18.0% | 62.2% | 带宽喂不饱(工位也没坐满) |
| f16x2 | **92.8%** | 284.1µs | 19.4% | 92.8% | 向量化后带宽近满 → 快 1.75× |
| f16x8 | 89.5% | 281.4µs | 14.7% | 95.2% | 带宽已到顶，再宽没意义 |
| f16x8_pack | 88.4% | **273.3µs** | **5.1%** | 97.1% | 带宽相近时**指令少者赢** |

三行结论：①朴素版输在带宽利用率和占用率；②x2 一步把带宽打到 93%（合并访存的力量，
G03 主题）；③带宽到顶后，竞争转向**指令条数**（Compute 5.1% = 打包技巧省了 3/4 指令）。

### 4.4 两个机制认知

- **为什么一次剖析要 8 个 pass**：ncu 靠"重放"采集——把 kernel 反复跑多遍、每遍采一组计数器
  （计数器比寄存器还稀缺，一次采不全）。所以被剖析的 kernel 变慢是正常的，**计时以它报的
  Duration 为准，别用剖析时的墙钟**。
- **报告很肥**：`--set full` 单份数百 MB。习惯：临时分析直接屏幕输出（不 `-o`）；
  落盘的分析完立刻删（本手册用户已被 17G 缓存教育过一次）。

## 五、诊断决策树（程序慢了怎么办）

```
程序慢
 └→ ① nvitop 看现在: GPU-Util 接近 0? → 根本没在用 GPU(同步bug/数据在CPU)
 │                     显存爆? → 复用分配, 别在循环里 cudaMalloc
 └→ ② nsys 看全程: 大头在 API(分配/拷贝)? → 优化搬运和分配策略
 │                  大头在 kernel? → 记下名字进③
 └→ ③ ncu 钻那个 kernel: 带宽高算力低 → memory-bound(少搬/合并/向量化)
                         带宽低占用低 → 排程问题(块太小/寄存器太多)
                         算力高带宽低 → compute-bound(算法/tensor core)
```

## 六、踩坑实录（本机全踩过）

1. **WSL 的 ERR_NVGPUCTRPERM**：性能计数器默认锁——Windows 控制面板解锁 +
   `wsl --shutdown`（✅ 2026-09-19 已完成，步骤在 `cuda_interview_notes/4060_可学清单.md`）。
   改完控制面板**必须重启 WSL**，否则错误换一副面孔继续骗你。
2. **lock 文件残留**：剖析中断后 `/tmp/nsight-compute-lock` 残留会挡住 root 运行——`rm` 掉即可。
3. **f32 系 kernel 剖析没输出**：`-k` 精确名匹配不到时改用 `-k "regex:名字片段"`。
4. **nsys 报告没 kernel 数据**：加 `-t cuda` 显式声明追踪 CUDA；程序太短也可能采不到，
   循环多跑几轮再采。

## 七、自测题

1. GPU-Util 显示 0% 但程序在跑，最可能什么问题？（kernel 没真启动/全在 CPU 端——先查
   `cudaGetLastError`，G02 的习惯）
2. ncu 显示某 kernel Memory 90% / Compute 20%——优化方向？（memory-bound：减少数据量、
   向量化访存、合并 io；优化计算白费）
3. nsys 显示 cudaMalloc 占 99% 时间——优化方向？（预分配+复用；分配是毫秒级昂贵操作）
4. 为什么 ncu 剖析时程序明显变慢、但报的 Duration 却可信？（多 pass 重放采集，Duration
   来自计数器换算而非墙钟）

动手：把 `labs/G03_index` 的行读/列读实验各跑一遍 ncu，对比两份报告的 Memory Throughput
——亲眼看看 93.8 vs 45.2 GB/s 在 ncu 里长什么样。
