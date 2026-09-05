# NET-05 链路带宽量级表（半测半背，infra 面试的底牌）

## 面试口述版（这张表要张口就来）

```
NVMe SSD               ~7 GB/s        盘比网快
本机回环 TCP            ~5-20 GB/s     走协议栈, 慢于共享内存(实测 1.2GB/s 管道对照 OS-05)
Host DRAM               ~50-100 GB/s   服务器多通道聚合
PCIe 4.0 x16            ~32 GB/s       4060 Laptop 是 x8 → ~16GB/s, 实测 8-14
PCIe 5.0 x16            ~64 GB/s
IB/RoCE 400Gb           ~50 GB/s       RDMA 绕内核, 训练集群跨机主力
NVLink 4.0              ~900 GB/s/卡   H100 卡间, 是 PCIe 的 15-30 倍
NVSwitch                ~1.6+ TB/s     8 卡全互联, AllReduce 不再"过树"
显存带宽                GDDR6 ~500GB/s / HBM3e 3.35TB/s  kernel 的屋顶
```

两个数量级直觉：**卡间 NVLink vs 跨机网络 = 20 倍**——所以大模型并行策略里 TP（高频通信）放机内、
PP/DP（低频通信）跨机；**PCIe vs NVLink = 20 倍**——所以 NCCL 拓扑探测的首要问题就是
"这两张卡之间有没有 NVLink bridge / 是不是 NVSwitch 全互联"。

## 面试叙事模板

"这些数字决定 NCCL 的一切决策：单机优先 P2P 走 NVLink，不支持 P2P 的组合退化成 SHM
（经 host 内存，联动 OS-05），跨机走 RDMA（联动 NET-04 零拷贝的终极形态）。而通信能不能被计算藏住，
就是比'计算耗时'和'数据量 ÷ 链路带宽'谁大——这正是我 overlap 课题 cost model 的底表。"
——把背表讲成研究日常，这是你和其他候选人的差别。

## 动手绑定

`demos/net5_bandwidth.cu`：本机能测的三项现场测（DRAM memcpy / PCIe H2D / D2H），其余引用公认量级。

```bash
nvcc -std=c++17 -O2 -arch=sm_89 demos/net5_bandwidth.cu -o /tmp/net5 && /tmp/net5
# 本机实测: DRAM ~2GB/s(WSL 虚拟化偏低) / PCIe H2D 8-10 / D2H 13-14 GB/s
# 对照: 与 stream_overlap.cu 的 13.3 GB/s 互相印证
```

注意口径：实测数字依赖机器（WSL2 虚拟化明显拉低），**面试引用公认量级 + 说明你实测过本机**，两者搭配最有说服力。

## 记忆钩子

"盘 7、网 50、PCIe 32/64、NVLink 900、Switch 上 T；TP 关机内，PP 跨机走，带宽差 20 倍。"

**人话版**：前半句是链路带宽背诵表（单位 GB/s）：NVMe 盘 ~7、IB 网络 ~50、PCIe 一代 32/五代 64、
**NVLink ~900**（卡对卡直连，比 PCIe 快 20-30 倍）、NVSwitch 上 T（8 卡全互联交换）。
后半句是并行策略的物理依据：**TP**（张量并行）每层都要通信，必须放在最快的 NVLink 上（同机内）；
**PP/DP**（流水线/数据并行）通信频率低，可以走慢的网络跨机——"带宽差 20 倍"就是切分依据，
也正是你 overlap 课题 cost model 的物理底表。
