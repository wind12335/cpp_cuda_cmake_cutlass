// 09 std::move 干了什么
// 运行: g++ -std=c++17 09_std_move.cpp -o t9 && ./t9
#include <cstdio>
#include <string>
#include <utility>

// 一个会"报账"的资源类: 拷贝/移动分别打印
class Buffer {
public:
    explicit Buffer(size_t n) : size_(n), data_(new char[n]) {
        printf("  构造(%zu bytes)\n", n);
    }
    Buffer(const Buffer& o) : size_(o.size_), data_(new char[o.size_]) {
        printf("  拷贝构造 —— 真的搬了 %zu bytes\n", size_);
    }
    Buffer(Buffer&& o) noexcept : size_(o.size_), data_(o.data_) {
        o.data_ = nullptr; o.size_ = 0;                 // 掠夺资源, 源置空
        printf("  移动构造 —— 只换指针, O(1)\n");
    }
    Buffer& operator=(Buffer&& o) noexcept {
        if (this != &o) { delete[] data_; data_ = o.data_; size_ = o.size_; o.data_ = nullptr; o.size_ = 0; }
        printf("  移动赋值\n");
        return *this;
    }
    ~Buffer() { delete[] data_; }
private:
    size_t size_ = 0;
    char*  data_ = nullptr;
};

int main() {
    printf("--- 1. 拷贝初始化 ---\n");
    Buffer a(1024);
    printf("拷贝 a:\n"); Buffer b = a;            // 拷贝构造

    printf("--- 2. std::move(a) ---\n");
    printf("move a:\n"); Buffer c = std::move(a); // 移动构造: std::move 只是标记, 干活的是移动构造

    printf("--- 3. const 对象 move 静默退化成拷贝 ---\n");
    const Buffer d(256);
    printf("move d:\n"); Buffer e = std::move(d); // const T&& 匹配不上移动构造 → 拷贝构造!

    printf("--- 4. 移动后源对象: 有效但别假设内容 ---\n");
    std::string s = "hello cuda";
    std::string t = std::move(s);
    printf("t = \"%s\", s.size() = %zu (空, 但标准不承诺)\n", t.c_str(), s.size());
    s = "reuse ok";                                // 可以重新赋值使用
    printf("s 重新赋值 = \"%s\"\n", s.c_str());
    return 0;
}
