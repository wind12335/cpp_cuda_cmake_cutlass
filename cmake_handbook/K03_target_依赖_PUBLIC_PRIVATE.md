# K03 target 与依赖：PUBLIC / PRIVATE / INTERFACE 【核心】

> 现代 CMake 的核心思想：**一切围绕 target**。include 路径、宏、链接库都"挂"在目标上，
> 依赖关系自动传递——这是读懂任何现代 CMakeLists 的钥匙。

## §K03.1 三种目标

```cmake
add_executable(app main.cu)              # 可执行目标
add_library(kernels STATIC a.cu b.cu)    # 静态库(.a): 编进最终可执行文件
add_library(tool SHARED tool.cu)         # 动态库(.so): 运行时加载
add_library(header_only INTERFACE)       # 纯头文件库: 没东西可编, 只有"配置"可传递
```

## §K03.2 target_include_directories 与 PUBLIC/PRIVATE/INTERFACE

**作用**：给目标加头文件搜索路径。三个关键字决定"这份设置要不要传递给链接我的人"：

| 关键字 | 我自己能用吗 | 链接我的人能用吗 |
|--------|------------|----------------|
| PRIVATE | ✓ | ✗ |
| INTERFACE | ✗ | ✓ |
| PUBLIC | ✓ | ✓（= PRIVATE + INTERFACE） |

```cmake
add_library(kernels STATIC src/reduce.cu)
target_include_directories(kernels
    PUBLIC  include           # 我的头文件在 include/ —— 链接我的人也需要它 → PUBLIC
    PRIVATE src/internal)     # 内部实现细节只有我自己用 → PRIVATE
```

**判断口诀**：出现在**公开头文件**里的 include → PUBLIC；只在 .cpp 实现里用的 → PRIVATE。

## §K03.3 target_link_libraries：依赖的"传染性"

**作用**：链接库，同时**传递**它的 PUBLIC 配置。

```cmake
add_executable(bench main.cu)
target_link_libraries(bench PRIVATE kernels)
# kernels 的 PUBLIC include 会自动传给 bench —— bench 里直接 #include "kernels/reduce.cuh" 即可
```

**传染链示例**：`bench → kernels(PUBLIC include/) → cutlass(INTERFACE include/)`
——bench 最终也能用 cutlass 的头文件，这就是 LeetCUDA/现代工程的依赖组织方式。

## §K03.4 target_compile_options / compile_definitions

**作用**：给目标加编译选项和宏。

```cmake
target_compile_options(kernels PRIVATE
    $<$<COMPILE_LANGUAGE:CUDA>:-O3 --generate-line-info>)   # 生成器表达式: 只对 CUDA 文件生效

target_compile_definitions(bench PRIVATE ENABLE_PROF)       # 等价代码里 #define ENABLE_PROF
```

【坑】不加生成器表达式 `$<$<COMPILE_LANGUAGE:CUDA>:...>` 直接给混合工程加 `-O3` 之类
nvcc 专用选项，会让 host 编译器报错。

## 本篇实验

`labs/03_targets/`——静态库 + include 传递 + 可执行文件的完整三件套（已验证构建运行）：

```bash
cd labs/03_targets
cmake -B build && cmake --build build && ./build/app
```
