# NET-04 零拷贝：read/write → mmap → sendfile

## 面试口述版

传统"读文件发出去"的完整路径有 **4 次数据拷贝 + 4 次用户/内核切换**：
磁盘→页缓存（DMA）→用户缓冲（CPU）→socket 缓冲（CPU）→网卡（DMA）——中间两次 CPU 拷贝纯属搬运工。
（**页缓存 page cache** = 内核在内存里给磁盘数据做的缓存：读文件先读进这里，写文件也先写到这里，
之后再择机落盘——所以"磁盘读写"实际大多是在和这块内存打交道。）
三种优化逐级砍：
① **mmap + write**：把页缓存直接映射进用户态，省掉"内核→用户"那次 CPU 拷贝（仍有一次切换与拷贝）；
代价是缺页/TLB 管理，**小文件不划算**；
② **sendfile**：数据全程在内核里从页缓存搬到 socket 缓冲，用户态只传文件描述符——零用户态拷贝，
Kafka/Nginx 静态文件的看家本领；
③ 终极形态是**带 DMA 收集的 sendfile**（scatter-gather）：连"页缓存→socket 缓冲"的内核拷贝都省了，
只传缓冲区描述符，网卡 DMA 直接分散收集。
实测（256MB，内存盘）：read+write 264ms → mmap+write 164ms → sendfile 22ms，**12 倍**。

## 高频追问

- **什么时候 sendfile 无效？** 需要在用户态处理数据（加密/改写）时必须先进用户态——零拷贝只在"原样搬运"场景成立。
- **和 GPUDirect 什么关系？（infra 杀招）** 同一思想的第三次跃迁：sendfile 省掉的是"CPU 内存里的搬运"，
GPUDirect RDMA 再省掉"经 CPU 内存"本身——网卡 DMA 直达 GPU 显存。NCCL 跨机 AllReduce 的带宽
能逼近线缆极限，靠的就是它。三层递进一句话：传统四拷贝 → 内核内直搬 → 显存直达到网卡。
- **Kafka 为什么快？** 顺序写页缓存 + sendfile 零拷贝消费，两条都踩在"减少拷贝与陷入"上（联动 OS-06）。

## 动手绑定

`demos/net4_zero_copy.c`：同一份 256MB 文件，三种搬运方式的耗时对比（含 socket 排水线程模拟发送）。

```bash
g++ -O2 -pthread demos/net4_zero_copy.c -o /tmp/net4 && /tmp/net4
# 实测: 264ms → 164ms → 22ms
```

## 记忆钩子

"四拷贝两多余；mmap 省一拷，sendfile 全内核，GPUDirect 连内存都不过。"

**人话版**：传统"读文件→发出去"要**四次拷贝**：磁盘→内核缓存（DMA）→用户缓冲区（CPU）→
socket 缓冲（CPU）→网卡（DMA），其中中间两次 CPU 拷贝纯属多余。逐级优化：**mmap** 把内核缓存
直接映射给用户看，省一次拷贝；**sendfile** 数据全程留在内核里直搬，用户态只递文件描述符；
**GPUDirect** 更进一步——连 CPU 内存都不经过，网卡 DMA 直接读写 GPU 显存（NCCL 跨机的底座）。
实测 256MB：264ms → 164ms → 22ms。
