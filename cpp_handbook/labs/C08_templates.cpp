// C08 实验: 模板与泛型
// 运行: g++ -std=c++17 -O2 C08_templates.cpp -o c08 && ./c08
#include <cstdio>

// §C08.1 函数模板
template <class T>
T maxOf(T a, T b) { return a > b ? a : b; }

// §C08.2 类模板
template <class T>
class Box {
public:
    explicit Box(T v) : v_(v) {}
    T get() const { return v_; }
private:
    T v_;
};

// §C08.3 值模板参数
template <int N>
struct FixedArray {
    int data[N] = {};
    constexpr static int size() { return N; }
};

// §C08.4 全特化
template <class T> const char* typeName() { return "unknown"; }
template <> const char* typeName<bool>() { return "bool"; }
template <> const char* typeName<int>() { return "int"; }

// §C08.5 CRTP
template <class D>
struct Base { void run() { static_cast<D*>(this)->run_impl(); } };
struct Fast : Base<Fast> { void run_impl() { printf("  CRTP run (编译期已定, 可内联)\n"); } };

// §C08.6 functor + 模板 = "CUDA kernel 元素级操作"的 CPU 预演
template <class F>
float apply(F f, float x) { return f(x); }
struct Scale {
    float k;
    explicit Scale(float k) : k(k) {}
    float operator()(float x) const { return x * k; }
};

int main() {
    printf("maxOf(3,5)=%d, maxOf(1.5,2.5)=%.1f\n", maxOf(3, 5), maxOf(1.5, 2.5));
    Box<int> a(1); Box<double> b(2.5);
    printf("Box<int>=%d, Box<double>=%.1f (两个独立类型)\n", a.get(), b.get());
    FixedArray<256> arr;
    printf("FixedArray<256>::size()=%d (编译期常数)\n", arr.size());
    printf("typeName: %s %s %s\n", typeName<int>(), typeName<bool>(), typeName<double>());
    Fast f; f.run();
    printf("apply(Scale(3), 5)=%.1f, apply(lambda, 5)=%.1f\n",
           apply(Scale(3.0f), 5.0f), apply([](float x) { return x + 1; }, 5.0f));
    return 0;
}
