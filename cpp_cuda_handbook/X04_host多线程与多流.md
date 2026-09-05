# X04 Host 多线程与多流 【进阶】

> 场景：一个线程不停往显存"喂"下一批数据（预取），另一个线程调度计算——重叠 I/O 与计算。
> 【前置：C09 并发】【CUDA G06 流与事件】

## §X04.1 基本事实

- CUDA runtime 是**线程安全**的：多个 host 线程可以同时调用 cuda API（同一 context 内会正确串行化）；
- **每个 host 线程配自己的流**是标准做法——线程内把任务排进自己的流，流之间自然并发；
- 大批量预取的内存必须 **pinned**（否则 async 假异步，前置：G04.4）。

## §X04.2 双线程流水线：生产者预取 + 消费者计算

**作用**：CPU 预取和 GPU 计算完全并行——数据"已经在显存里等着"。

```cpp
std::vector<DeviceBuffer> stages(2);        // 双缓冲(前置 X01)
cudaStream_t copyStream, computeStream;
cudaStreamCreate(&copyStream); cudaStreamCreate(&computeStream);

std::thread producer([&] {                  // CPU 线程: 不停准备+上传
    for (int i = 0; i < nBatches; ++i) {
        prepareHostData(host_stage[i % 2], i);
        cudaMemcpyAsync(stages[i % 2].get(), host_stage[i % 2].get(),
                        bytes, H2D, copyStream);
        cudaStreamSynchronize(copyStream);   // 确保这一批已经到了显存
        ready[i] = true;                     // 标记(实际工程用原子/条件变量)
    }
});
// GPU 线程(main): 等一批算一批
for (int i = 0; i < nBatches; ++i) {
    while (!ready[i]) std::this_thread::yield();
    kernel<<<grid, block, 0, computeStream>>>(stages[i % 2].get());
    cudaStreamSynchronize(computeStream);
}
```

## §X04.3 单线程也能重叠：事件驱动的三段流水线

**作用**：其实不需要多线程！多个流 + 事件就够（G06 的切块模式扩展）：

```
s_copy:  [H2D 0][H2D 1][H2D 2] ...
s_calc:        [K 0]  [K 1]  [K 2] ...
依赖:    kernel_i 在流 s_calc 里, 先 cudaStreamWaitEvent(s_calc, copyDone_i)
```

**什么时候才需要多线程**：host 端本身也要做重计算（预处理、后处理），或者要与 GPU 异步做
CPU 密集任务（日志、监控）。单纯"喂 GPU"用流+事件即可。

## §X04.4 多线程共享 CUDA 上下文的注意事项

1. 第一次 CUDA 调用会隐式创建 context——让**主线程先做一次初始化**（如 cudaFree(0)），
   避免多线程同时创建的竞态；
2. 每个 host 线程记住自己"当前设备"（`cudaSetDevice`）——多卡程序里线程切换设备要显式设置；
3. 回调（`cudaLaunchHostFunc`）运行在 CUDA 内部线程，里面不要调 cuda API。

## 本篇实验

`labs/X04_threads_streams.cu`——双缓冲 + 生产者线程 + GPU 计算线程，对比"不重叠"版本的总耗时。
