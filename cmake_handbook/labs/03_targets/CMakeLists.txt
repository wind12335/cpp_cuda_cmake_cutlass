cmake_minimum_required(VERSION 3.18)
project(targets_lab LANGUAGES CXX CUDA)
if(NOT CMAKE_CUDA_ARCHITECTURES)
    set(CMAKE_CUDA_ARCHITECTURES 89)
endif()

# 现代 CMake 找 CUDA 的标准姿势(比手写 -I/usr/local/cuda/include 优雅且可移植)
find_package(CUDAToolkit REQUIRED)     # 找不到会报清晰错误; 提供 CUDA::cudart 等目标

# 目标 1: kernel 静态库 (K03: PUBLIC include 传递给链接者)
add_library(mykernels STATIC src/mykernels.cu)
target_include_directories(mykernels PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(mykernels PRIVATE
    $<$<COMPILE_LANGUAGE:CUDA>:-O3 --generate-line-info>)   # 只对 .cu 生效(K03.4)

# 目标 2: 可执行文件 (纯 .cpp, 由 g++ 编译!)
# ⚠ 它用了 cuda_runtime.h + cudaMalloc —— 所以必须链接 CUDA::cudart:
#   该目标自带 include 路径 + 库链接, 上面的报错正是因为缺了它
add_executable(app app.cpp)
target_link_libraries(app PRIVATE mykernels CUDA::cudart)
