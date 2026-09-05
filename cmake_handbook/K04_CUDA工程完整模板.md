# K04 CUDA 工程完整模板（LeetCUDA 同款）【核心】

## §K04.1 最小 CUDA 工程（K01 + CUDA 语言）

```cmake
cmake_minimum_required(VERSION 3.18)          # CUDA 支持建议 3.18+
project(gemm_lab LANGUAGES CXX CUDA)          # ← CUDA 作为语言, .cu 交给 nvcc

if(NOT CMAKE_CUDA_ARCHITECTURES)              # 缓存变量没被 -D 指定时才设默认
    set(CMAKE_CUDA_ARCHITECTURES 89)          # 4060=89 (86=30系, 90=H100)
endif()

add_executable(gemm main.cu)
target_compile_options(gemm PRIVATE
    $<$<COMPILE_LANGUAGE:CUDA>:-O3 --generate-line-info>)
```

【坑】`CMAKE_CUDA_ARCHITECTURES` 不设的话，CMake 默认编一堆架构（配置+编译都慢好几倍）。
常见编号：70=V100, 75=2080Ti/T4, 80=A100, **86=RTX30 系, 89=RTX40 系(你的 4060), 90=H100**。

## §K04.2 实战报错实录：纯 .cpp 用 CUDA API 必须链接 CUDA::cudart

**报错**（labs/03_targets 初版真实撞上）：

```
app.cpp: fatal error: cuda_runtime.h: No such file or directory
```

**原因**：`app.cpp` 是纯 C++ 文件，由 g++ 编译——CUDA 的头文件路径和库对它**不可见**
（它们只挂给 CUDA 语言的目标）。**解法**（现代 CMake 标准姿势）：

```cmake
find_package(CUDAToolkit REQUIRED)          # 找到 CUDA 工具链(提供 CUDA::cudart 目标)

add_executable(app app.cpp)                 # 纯 .cpp 也要用 cudaMalloc? 必须链 cudart:
target_link_libraries(app PRIVATE mykernels CUDA::cudart)
# CUDA::cudart 自带 include 路径 + 库链接, 一行解决 (lab 03_targets 已验证)
```

## §K04.3 LeetCUDA 工程结构对照

```
kernels/hgemm/
├── CMakeLists.txt        ← 你现在应该能读懂: project(…CUDA) / add_executable /
├── makefile                target_link_libraries / CMAKE_CUDA_ARCHITECTURES
├── naive/ mma/ wgmma/    ← 各代 kernel 源码 (cuda_handbook G07 的三层进化)
├── pybind/               ← pybind11 绑定(pybind11_handbook 02 详解)
└── tools/ utils/         ← 公共工具
```

读任何 CMakeLists.txt 的三问：①有哪些 add_executable/add_library？②每个 target
link 了什么？③PUBLIC 配置会传染给谁？——三问答完，工程结构就清楚了。

## §K04.4 可选：CMake 里写测试

```cmake
enable_testing()
add_executable(test_reduce test_reduce.cu)
target_link_libraries(test_reduce PRIVATE mykernels)
add_test(NAME reduce_correct COMMAND test_reduce)    # 测试程序返回 0 = 通过
```

```bash
ctest --test-dir build          # 一键跑全部测试 (CI 友好)
```

## 本篇实验

`labs/02_cuda_project/`（最小 CUDA 工程）+ `labs/03_targets/`（多目标 + find_package），
均已构建运行验证。
