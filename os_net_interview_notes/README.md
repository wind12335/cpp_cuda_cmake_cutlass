# OS / 网络 八股 12 题 —— 讲解 + 可运行实测

与 `cpp_interview_notes/`、`cuda_interview_notes/` 配套的第三套。这一批的特点：
**几乎全部可以亲手实测**——OS 7 题全部配 demo（微基准），网络 5 题里 3 题配 demo、1 题半 demo、1 题纯背诵表。

每题一个 `.md`：**60-90 秒口述版 → 高频追问 → 动手绑定（编译命令）→ 记忆钩子**。
每个 demo 都已在本机（WSL2 + RTX 4060）编译运行验证，md 里记录了实测数字。

## 总表

### OS_7题（全部可实测）

| # | 题目 | demo | 实测亮点 |
|---|------|------|---------|
| 01 | 进程/线程/协程 | `os1_threads_forks.c` | 线程 ~100μs vs fork ~240μs |
| 02 | 虚拟内存/缺页 | `os2_vm_pgfault.c` | mmap 0.003ms / 首触 190ms(65538 缺页) / 复访 10ms |
| 03 | 互斥锁 vs 自旋锁 | `os3_mutex_vs_spin.c` | 无竞争 ~20ns，高竞争对照 |
| 04 | 死锁四条件 | `os4_deadlock.cpp` | 子进程造死锁→确诊→kill→两解 |
| 05 | IPC/共享内存 | `os5_shm_ipc.c` | **shm 比管道快 80 倍**（NCCL SHM 底座）|
| 06 | 用户态/内核态 | `os6_syscall_vdso.c` | vDSO 15ns vs syscall 167ns |
| 07 | 缓存层级/MESI | `os7_cache_levels.c` | 四级悬崖 236→144→75→29 GB/s |

### 网络_5题（3 demo + 1 半 + 1 背诵）

| # | 题目 | demo | 实测亮点 |
|---|------|------|---------|
| 01 | TCP 握手/TIME_WAIT | `net1_tcp_timewait.c` | 亲手造出 22 个 TIME_WAIT |
| 02 | TCP vs UDP | `net2_tcp_vs_udp.c` | 吞吐/RTT/丢包三合一 |
| 03 | epoll LT/ET | `net3_epoll_lt_et.c` | LT 连报 3 次 vs ET 报 1 次 |
| 04 | 零拷贝 | `net4_zero_copy.c` | **264ms→164ms→22ms（12 倍）** |
| 05 | 带宽量级表 | `net5_bandwidth.cu` | 本机测 PCIe/DRAM + 全链路背诵表 |

## 使用方法

```bash
# 一键编译检查全部 demo
bash check_all.sh

# 单个运行(示例)
g++ -O2 -pthread demos/os5_shm_ipc.c -o /tmp/os5 && /tmp/os5
```

1. 每天 2-3 题：读 md → 跑 demo 看数字 → 合上口述。
2. demo 里的"GPU/NCCL 联动"注释是**你独有的面试素材**——把 OS/网络知识和你的课题接起来的那句话，面试时主动说。
3. 网络题在 WSL2 上数字偏保守（回环/内存都虚低），md 已标注；引用时以"量级 + 实测过"的口径讲。

## 编译口径

- C/C++ demo：`g++ -O2 -pthread 文件 -o xxx`（个别需要 `<atomic>` 等头文件已带）
- CUDA demo（net5）：`nvcc -std=c++17 -O2 -arch=sm_89 文件 -o xxx`

## 制作过程中踩到的真 bug（值得看一眼）

- 阻塞 pipe 读空后 `read` 永久挂起（net3 demo 初版卡死）——正是"ET 必须非阻塞"的原因；
- 死锁 demo 里"修复版"复用被死锁线程占住的锁反而又死锁——换成子进程造死锁、父进程 kill；
- UDP 接收端等一个永远不来的包会无限阻塞——必须 `SO_RCVTIMEO`。

这三个都是"跑起来才知道"的活教材，面试聊到对应题目时可以直接当自己的故事讲。

## 📖 阅读约定

- 每题的 **"记忆钩子"** 是压缩口诀，只给理解后的人当回忆开关——先读正文和实测数字，
  懂了再用钩子自测（能从口诀还原出整段解释才算懂）。口诀下都配了 **"人话版"**（已全部补齐）。
- 正文遇到没解释的术语 = bug，报文件名+原句，我来补解释。
