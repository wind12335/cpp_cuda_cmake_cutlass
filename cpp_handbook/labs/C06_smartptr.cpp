// C06 实验: 智能指针全家桶
// 运行: g++ -std=c++17 -O2 C06_smartptr.cpp -o c06 && ./c06
#include <cstdio>
#include <memory>
#include <vector>

// 一个会"报账"的资源类: 观察 unique/shared/weak 各自何时创建销毁
struct Res {
    int id;
    explicit Res(int id) : id(id) { printf("  Res(%d) 创建\n", id); }
    ~Res() { printf("  Res(%d) 销毁\n", id); }
};

int main() {
    // ---- unique_ptr: 独占 ----
    printf("== unique_ptr ==\n");
    auto u = std::make_unique<Res>(1);
    // auto u2 = u;                    // ✗ 打开注释: 编译错(禁止拷贝)
    auto u2 = std::move(u);            // 转移所有权
    printf("  u 现在%s, u2 持有 Res(1)\n", u ? "非空" : "空");

    // ---- shared_ptr: 共享 ----
    printf("== shared_ptr ==\n");
    auto s = std::make_shared<Res>(2);
    {
        auto s2 = s;                            // 拷贝: 强计数+1
        printf("  use_count=%ld\n", s.use_count());
    }
    printf("  use_count=%ld (离开作用域自动-1)\n", s.use_count());

    // ---- weak_ptr: 观察(不加强计数) + 死后 lock 给空 ----
    printf("== weak_ptr ==\n");
    std::weak_ptr<Res> w = s;
    printf("  观察: 强=%ld 弱=%ld\n", s.use_count(), w.use_count());
    if (auto sp = w.lock()) printf("  lock 成功: 对象活着\n");
    s.reset();                                  // 对象销毁
    printf("  对象已销毁, 再 lock: %s\n", w.lock() ? "成功" : "失败(返回空)");

    // ---- 实战: 自定义删除器管"非 new 资源"(CUDA 语境的前置) ----
    printf("== 自定义删除器 ==\n");
    std::shared_ptr<FILE> file(fopen("/tmp/c06_demo.txt", "w"), [](FILE* f) {
        if (f) { fclose(f); printf("  [deleter] 文件已自动关闭 (RAII!)\n"); }
    });
    if (file) fprintf(file.get(), "hello\n");
    // 离开作用域自动 fclose —— 忘 fclose 导致的句柄泄漏从此不可能
    return 0;
}
