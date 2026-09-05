# 01 pybind11 最小模块 【基础】

## §1.1 pybind11 是什么

**作用**：让 Python 直接调用你的 C++/CUDA 代码——把 C++ 函数/类"绑定"成 Python 模块。
PyTorch、vLLM 的自定义算子、LeetCUDA 的所有 kernel，暴露给 Python 用的都是它（torch 内置）。

```
Python: import mymod; mymod.add(1, 2)
             ↑ pybind11 做类型转换/函数分发
C++:    int add(int a, int b) { return a + b; }
```

## §1.2 最小模块（三行核心）

```cpp
// add.cpp
#include <pybind11/pybind11.h>
namespace py = pybind11;

int add(int a, int b) { return a + b; }

PYBIND11_MODULE(add, m) {              // 模块名 = add (必须和编译出的文件名一致!)
    m.def("add", &add, "两数相加",
          py::arg("a"), py::arg("b")); // 参数名让 Python 可以 add(a=1, b=2)
}
```

## §1.3 编译与使用（两种方式）

```bash
# 方式 A: 直接命令行编译 (快速验证)
c++ -O3 -shared -std=c++17 -fPIC \
    $(python3 -m pybind11 --includes) \
    add.cpp -o add$(python3-config --extension-suffix)
#   $(python3 -m pybind11 --includes)     → Python/pybind 头文件路径
#   $(python3-config --extension-suffix)  → .so 的后缀(如 .cpython-314.so)

# 方式 B: pybind11 官方构建助手 (更省事, 自动加 -fPIC 等)
c++ -O3 -shared -std=c++17 $(python3 -m pybind11 --includes) add.cpp -o add$(python3-config --extension-suffix)

python3 -c "import add; print(add.add(1, 2))"   # 3
```

## §1.4 常用绑定功能速查

| 绑定 | 写法 | Python 侧效果 |
|------|------|--------------|
| 函数 | `m.def("f", &f)` | `m.f(...)` |
| 带默认参数 | `py::arg("x") = 3` | `m.f()` 用默认 |
| 关键字调用 | `py::arg("a"), py::arg("b")` | `m.f(a=1, b=2)` |
| 类 | `py::class_<Dog>(m, "Dog")` | `d = m.Dog()` |
| 成员函数 | `.def("bark", &Dog::bark)` | `d.bark()` |
| 成员变量 | `.def_readwrite("name", &Dog::name)` | `d.name = "x"` |
| STL 容器 | `#include <pybind11/stl.h>` | list/dict 自动互转 |

## §1.5 本篇实验

`labs/01_add/`——一个带自定义删除器管 GPU 显存的完整绑定（pybind + CUDA 合体预告），
已在本机编译并通过 `import` 验证。
