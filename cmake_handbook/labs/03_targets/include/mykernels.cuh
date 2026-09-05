#pragma once                 // 防止头文件被重复包含(现代写法, 等价 include guards)
#include <cstdio>

// 库对外提供的接口: 一个简单的 GPU 规约(求和)
float gpu_sum(const float* d, int n);
