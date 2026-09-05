cmake_minimum_required(VERSION 3.18)
project(gemm_lab LANGUAGES CXX CUDA)

if(NOT CMAKE_CUDA_ARCHITECTURES)
    set(CMAKE_CUDA_ARCHITECTURES 89)
endif()

add_executable(gemm main.cu)
target_compile_options(gemm PRIVATE
    $<$<COMPILE_LANGUAGE:CUDA>:-O3 --generate-line-info>)
