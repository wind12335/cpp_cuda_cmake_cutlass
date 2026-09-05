# 10 Kernel Launch 失败排查三板斧

## 面试口述版

CUDA 的错误是**异步的、滞后的**：非法访存往往在崩溃点之后的好几个 API 才报出来，所以排查要有套路。
**第一板斧：错误码检查习惯**。每个 API 返回值检查；kernel launch 后跟
`cudaGetLastError()`（取错误并清除）——launch 参数错误（block 超上限 1024、shared 超容量）在这里当场暴露；
拷贝后跟 `cudaMemcpy` 返回值。
**第二板斧：compute-sanitizer**。错误码只告诉你"炸了"，sanitizer 告诉你"哪行炸的"：
`--tool memcheck` 查非法访存/越界、`--tool racecheck` 查 shared memory 竞争/bank conflict、
`--tool initcheck` 查未初始化读。慢 10-50 倍，所以是调试工具不是性能工具。
**第三板斧：同步化定位**。怀疑异步错误错位时，设 `CUDA_LAUNCH_BLOCKING=1`（每个 launch 变同步），
让错误在真实位置报出；配合 `cuda-gdb` 或 printf 调试。nsys 时间线上也能看到"哪个 kernel 后面跟了错误"。
口诀："**错误码当场查、sanitizer 定位行、同步化归位错**"。

## 高频追问

- **kernel 非法访存，为什么程序有时还能继续跑？** 错误标志挂起在 context 里，直到下一次 CUDA API 才返回——所以"后面莫名其妙的调用失败"其实是前面的 kernel 炸了。
- **printf 调试 kernel 行吗？** 行（`printf` 在 device 端可用，输出有缓冲、顺序不保证），注意它会拖慢 kernel，只在调试时用。
- **常见的 launch 配置错误？** block 超 1024 线程；shared 超过 48KB 且没 `cudaFuncSetAttribute` 申请；grid 维度 0；`__shared__` 在递归/动态大小计算错。
- **和 NCCL 排查的关系？** 同一套思维：NCCL hang/报错时也是"开 NCCL_DEBUG=INFO 看卡在哪步 + nsys 时间线看 collective 有没有齐"——把 CUDA 排查习惯迁移到通信栈（你的实战强项）。

## 动手绑定

配套 demo `demos/launch_error.cu`（故意触发越界，演示完整排查链）：

```bash
nvcc -std=c++17 -arch=sm_89 demos/launch_error.cu -o /tmp/le && /tmp/le
compute-sanitizer --tool memcheck /tmp/le
```

先看程序自己的 `cudaGetLastError` 报了什么，再用 sanitizer 定位到行。

## 记忆钩子

"launch 后查 LastError，可疑就上 sanitizer，错位就 LAUNCH_BLOCKING。"

**人话版**：三件排查工具按顺序上——①**每次 kernel 启动后**调 `cudaGetLastError()`，当场抓配置类
错误（线程数超限、共享内存超量）；②怀疑内存越界/竞争时用 **compute-sanitizer**（memcheck 查越界、
racecheck 查竞争），能精确报告到代码行，代价是慢几十倍；③CUDA 错误是**异步**的（炸了往往要等后面
某个 API 才报出来），设置环境变量 `CUDA_LAUNCH_BLOCKING=1` 让每次启动变同步，错误就会在真实
位置冒出来。
