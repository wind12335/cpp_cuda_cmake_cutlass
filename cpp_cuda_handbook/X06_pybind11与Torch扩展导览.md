# X06 pybind11 与 Torch 扩展导览 【工程，无 lab】

> 这是通往 LeetCUDA 工程形态的最后一课：**把你写的 kernel 暴露给 Python**。
> 本篇是导览（需要 pip 环境才能跑），代码模式直接对照 LeetCUDA 源码。

## §X06.1 为什么是 pybind11

**作用**：Python 调用你的 CUDA kernel 的标准桥梁。两个生态都靠它：
- **纯 pybind11**：`pip install pybind11`，把 C++/CUDA 函数编译成 Python 可 import 的模块；
- **PyTorch 扩展**：torch 内置 pybind11，`torch.utils.cpp_extension.load` 一行编译——
  **LeetCUDA 每个 kernel 目录的 `xxx.py` 就是这么加载 `xxx.cu` 的**。

## §X06.2 LeetCUDA 的加载模式（你天天见却可能没细看）

```python
# elementwise.py (LeetCUDA 原文模式)
from torch.utils.cpp_extension import load
lib = load(
    name="elementwise_lib",
    sources=["elementwise.cu"],                    # 你的 CUDA 源码
    extra_cuda_cflags=["-O3", "--expt-relaxed-constexpr", ...],
    verbose=False,
)
out = lib.elementwise_add_f32(a, b)                # 像调用普通 Python 函数
```

`.cu` 文件里必须有的三段：

```cpp
#include <torch/extension.h>                       // torch 的 pybind 封装
// ... 你的 kernel ...
// ① wrapper: 把 torch.Tensor 转成裸指针, 调 kernel
torch::Tensor elementwise_add_f32(torch::Tensor a, torch::Tensor b) {
    auto c = torch::empty_like(a);
    int N = a.numel();
    int block = 256, grid = (N + block - 1) / block;
    elementwise_add_f32_kernel<<<grid, block>>>(
        a.data_ptr<float>(), b.data_ptr<float>(), c.data_ptr<float>(), N);
    return c;
}
// ② 绑定: 告诉 pybind 哪些函数暴露给 Python
PYBIND11_MODULE(TORCH_EXTENSION_NAME, m) {
    m.def("elementwise_add_f32", &elementwise_add_f32, "a+b elementwise");
}
```

**关键映射**（Python 对象 ↔ C++ 对象）：
`torch.Tensor` ↔ `torch::Tensor`；`.data_ptr<float>()` 拿裸指针进 kernel；
显存分配用 `torch::empty_like`（走 PyTorch 的缓存分配器，**别在扩展里 cudaMalloc**）。

## §X06.3 纯 pybind11 模式（不依赖 torch）

```cpp
// add.cu
#include <pybind11/pybind11.h>
__global__ void add_kernel(const float* a, float* out, int n) { ... }

std::vector<float> add(std::vector<float> xs, float k) {   // STL 自动转 Python list
    // 分配显存 → 搬入 → kernel → 搬回 (X01/X02 的封装在这里复用!)
}
PYBIND11_MODULE(add, m) { m.def("add", &add); }
```

```bash
c++ -O3 -shared -std=c++17 -fPIC $(python3 -m pybind11 --includes) \
    add.cu -o add$(python3-config --extension-suffix) -L/usr/local/cuda/lib64 -lcudart
python3 -c "import add; print(add.add([1,2], 3.0))"
```

## §X06.4 调用链全图（把三门手册串起来）

```
Python: lib.elementwise_add(a, b)
   ↓ pybind11 (X06)
C++: elementwise_add(Tensor a, Tensor b)     ← torch::Tensor 包装(本手册 X01 思想)
   ↓ .data_ptr<float>()
C++: launch(wrapper_kernel, grid, block, ...) ← X02 启动包装器
   ↓ kernel<<<...>>>
CUDA: elementwise_add_kernel <<...>>>        ← G02/G03 索引与合并访存
```

面试时画得出这张图，"你了解 PyTorch 怎么调到 GPU 吗"就是送分题。

## §X06.5 动手路径

```bash
pip install torch --index-url https://download.pytorch.org/whl/cu128   # torch 自带 pybind11
cd ~/workspace/LeetCUDA/kernels/elementwise
python elementwise.py            # 真·运行 LeetCUDA 的 kernel 测试
```

读源码顺序建议：`elementwise/elementwise.py`（怎么加载）→ `elementwise.cu` 末尾
（wrapper + PYBIND11_MODULE）→ kernel 本体 → 对照 `cuda_handbook/G03`。
