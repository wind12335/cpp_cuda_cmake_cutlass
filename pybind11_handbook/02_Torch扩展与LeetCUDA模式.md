# 02 Torch 扩展与 LeetCUDA 模式 【工程】

> torch 内置了 pybind11，`torch.utils.cpp_extension.load` 把"编译+绑定"简化成一行——
> **LeetCUDA 每个 kernel 目录的 `xxx.py` 用的就是这个**。

## §2.1 torch.utils.cpp_extension.load 模式

```python
# elementwise.py —— LeetCUDA kernels/elementwise/ 的原文模式
import torch
from torch.utils.cpp_extension import load

lib = load(
    name="elementwise_lib",                 # 编译产物的模块名
    sources=["elementwise.cu"],             # CUDA 源码 (内含 PYBIND11_MODULE)
    extra_cuda_cflags=[
        "-O3",
        "--expt-relaxed-constexpr",
        "-U__CUDA_NO_HALF_OPERATORS__",     # 解锁 __half 的运算符
    ],
    verbose=False,
)
a = torch.randn(1024, device="cuda")
b = torch.randn(1024, device="cuda")
out = lib.elementwise_add_f32(a, b)         # 直接调用!
```

【机制】首次调用自动调 nvcc 编译成 .so 并缓存（`~/.cache/torch_extensions/`），
改了 .cu 会自动重编——"源码即安装"。

## §2.2 .cu 里的三段式（torch 版 pybind）

```cpp
#include <torch/extension.h>                // ① torch 的 pybind 封装(含 Tensor)

__global__ void kernel(...) { /* 你的 kernel */ }

// ② wrapper: torch::Tensor 进出, 裸指针进 kernel
torch::Tensor elementwise_add_f32(torch::Tensor a, torch::Tensor b) {
    auto c = torch::empty_like(a);          // 用 torch 的分配器(缓存+统一管理)!
    int N = a.numel();
    elementwise_add_f32_kernel<<<(N + 255) / 256, 256>>>(
        a.data_ptr<float>(), b.data_ptr<float>(), c.data_ptr<float>(), N);
    return c;
}

// ③ 绑定
PYBIND11_MODULE(TORCH_EXTENSION_NAME, m) {  // TORCH_EXTENSION_NAME 由 load() 注入
    m.def("elementwise_add_f32", &elementwise_add_f32);
}
```

【三个铁律】
1. 显存用 `torch::empty_like` / `torch::empty` 分配（走 PyTorch 缓存分配器），
   **绝不在扩展里 cudaMalloc/cudaFree**（和 torch 的显存池打架）；
2. `.data_ptr<float>()` 前确认 dtype/device 对（可用 `TORCH_CHECK(a.dtype()==torch::kFloat)` 加固）；
3. wrapper 结束前 `return c;`——tensor 的生命周期由 Python 侧引用计数管理。

## §2.3 调用链全图（三门手册在这里合流）

```
Python: lib.elementwise_add_f32(a, b)
  ↓ pybind11 (X06 / pybind11_handbook 01)
C++: torch::Tensor elementwise_add_f32(...)     ← torch::Tensor = RAII 显存(X01 思想的工业版)
  ↓ a.data_ptr<float>()
C++: kernel<<<grid, block>>>(...)               ← X02 启动包装器思想
  ↓
CUDA: elementwise_add_f32_kernel                ← G03 合并访存 + G04 shared + ...
```

## §2.4 动手路径（本机可执行）

```bash
pip install torch --index-url https://download.pytorch.org/whl/cu128   # torch 自带 pybind
cd ~/workspace/LeetCUDA/kernels/elementwise
python elementwise.py            # 真实运行 LeetCUDA 的 kernel 测试!
```

读源码顺序：`elementwise.py`（加载方式）→ `elementwise.cu` 末尾（wrapper+绑定）→ kernel 本体
→ 对照 `cuda_handbook/G03`。
