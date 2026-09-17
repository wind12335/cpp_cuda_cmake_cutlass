# ─────────────────────────────────────────────────────────
# 我的第一个 MLIR Pass —— 构建脚本
# 【前置标注】看懂本文件需要：cmake_handbook/K01(最小CMake)、K03(find_package)
#
# 人话版：这个文件告诉 cmake 三件事——
#   1) 去哪里找 MLIR（find_package，apt 装在 /usr/lib/llvm-15）
#   2) 头文件在哪（include_directories）
#   3) 把 MulToAdd.cpp 编成可执行文件 my-opt-rw，并链上 MLIR 的库
# ─────────────────────────────────────────────────────────
cmake_minimum_required(VERSION 3.13.4)
# ⚠️ 踩坑实录：必须写 "C CXX" 两个语言！只写 CXX 的话，LLVMConfig.cmake
# 内部会调用 check_c_source_compiles 检测 FFI/Terminfo，报错
# "check_source_compiles: C: needs to be enabled before use" 直接失败
project(my_rewrite_pass C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# LLVM/MLIR 源码默认关掉了 RTTI（为了省内存），我们保持一致，避免链接冲突
add_compile_options(-fno-rtti)

# MLIR 是 apt 装的，位置固定在 /usr/lib/llvm-15/lib/cmake/mlir
find_package(MLIR REQUIRED CONFIG
             PATHS /usr/lib/llvm-15/lib/cmake/mlir
             NO_DEFAULT_PATH)
message(STATUS "使用 MLIR 版本: ${LLVM_PACKAGE_VERSION}")

include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
include_directories(SYSTEM ${MLIR_INCLUDE_DIRS})
link_directories(${LLVM_BUILD_LIBRARY_DIR})

# 编译我们自己的"迷你 mlir-opt"工具：链上 MLIR 后它天生就认识
# --canonicalize 等所有官方 Pass，再加上我们自己注册的 --print-op-names
add_executable(my-opt-rw MulToAdd.cpp)
target_link_libraries(my-opt-rw PRIVATE
  MLIRMlirOptMain   # MlirOptMain：官方 mlir-opt 工具的主体，拿来复用
  MLIRPass          # Pass 基类/PassManager
  MLIRIR            # Operation/Value/Block 等核心数据结构
  MLIRParser        # 解析 .mlir 文本
  MLIRSupport       # 工具函数
  MLIRFuncDialect   # func dialect（FuncOp 级 Pass 需要）
)
