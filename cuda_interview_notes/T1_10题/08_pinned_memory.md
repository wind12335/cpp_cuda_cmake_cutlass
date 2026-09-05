# 08 Pinned Memory（锁页内存）

## 面试口述版

`cudaMallocHost`/`cudaHostAlloc` 分配的**页锁定**内存：物理内存不会被换出，虚拟地址固定。
两个用途：
① **真异步 H2D/D2H 的前提**——`cudaMemcpyAsync` 配普通 pageable 内存时，
驱动会先拷到一块内部 staging buffer（同步拷贝），async 名存实亡；配 pinned 内存才是 DMA 引擎直通的真异步。
（**DMA** = Direct Memory Access，"不经 CPU、由专用硬件自己搬数据"的引擎——CPU 只下命令，
数据搬运由 DMA 硬件完成，期间 CPU 可以干别的。）
② 更快的同步拷贝（省一次中转，通常快 30-50%）。
代价：pinned 内存**不可换页**，分配过量会挤压系统物理内存——只给"反复传输的热路径 buffer"用。
进阶：`cudaHostRegister` 把已分配的大缓冲原地锁页；`cudaHostAllocMapped` + zero-copy 让 GPU 直接透过 PCIe 访问 host 内存（带宽低，仅小数据/低频访问用）。
NCCL 语境：通信 buffer 全是 pinned/GPU 显存，这正是它和裸 socket 搬运的本质区别之一。

## 高频追问

- **cudaMemcpyAsync + pageable 一定是同步的？** 拷贝本身可能仍然排队异步执行，但驱动要先做一次 CPU 侧中转拷贝，流水线被打断——面试答"async 的效果被破坏"即可。
- **pinned vs unified memory？** pinned 是 host 侧锁页，CUDA 模型内显式管理；UM 是按需迁移的虚拟统一地址，方便但性能不可控，生产训练管线几乎不用。
- **和 GPUDirect 什么关系？** RDMA 网卡直取 GPU 显存（跳过 CPU）是 GPUDirect RDMA；pinned 解决的是"CPU↔GPU 这一段"的效率，两回事但同属"砍掉中转拷贝"的思想（我在 C++ 零拷贝题里讲过同构逻辑）。

## 动手绑定

LeetCUDA 没有专门 demo（它默认数据常驻显存）——我补了一个最小实验，见本目录 `demos/stream_overlap.cu`：

```bash
nvcc -std=c++17 -O2 -arch=sm_89 demos/stream_overlap.cu -o /tmp/so && /tmp/so
```

观察三组数字：pageable 同步拷贝、pinned 同步拷贝、pinned + 双流重叠——pinned 通常快 30-50%，双流再藏掉一部分。

## 记忆钩子

"锁页才能真异步；DMA 直通不中转；别锁太多挤爆主机。"

**人话版**：普通内存（pageable）可能被操作系统挪到磁盘，所以 GPU 来搬数据前驱动要先转一手——
`cudaMemcpyAsync` 配它就是假异步。**锁页（pinned）内存** = 向系统承诺"这块内存永不挪动"，
DMA 引擎（不经 CPU 的专用搬运硬件）可以直通搬运，async 才是真的。代价：锁页内存占着物理内存
不放，锁太多会把主机内存挤爆——只给反复传输的热路径缓冲用。
