// C06 实验: 智能指针全家桶
// 运行: g++ -std=c++17 -O2 C06_smartptr.cpp -o c06 && ./c06
#include <cstdio>
#include <memory>
#include <vector>
#include <string>
#include <cstdlib>

// §C06.5 计数器: 数"到底分配了几次堆"
static int allocs = 0;
void* operator new(size_t sz) { ++allocs; return std::malloc(sz); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, size_t) noexcept { std::free(p); }

// 一个会"报账"的资源类: 观察 unique/shared/weak 各自何时创建销毁
struct Res {
    int id;
    explicit Res(int id) : id(id) { printf("  Res(%d) 创建\n", id); }
    ~Res() { printf("  Res(%d) 销毁\n", id); }
};

// §C06.4.1 循环引用实验的两种节点: 唯一区别是 peer 的类型
struct BadNode {                     // shared 互指 → 循环引用 → 泄漏
    std::string name;
    std::shared_ptr<BadNode> peer;
    static int alive;
    BadNode(std::string n) : name(std::move(n)) { ++alive; }
    ~BadNode() { --alive; }
};
int BadNode::alive = 0;
struct GoodNode {                    // 一端 weak → 断开循环 → 正确析构
    std::string name;
    std::weak_ptr<GoodNode> peer;
    static int alive;
    GoodNode(std::string n) : name(std::move(n)) { ++alive; }
    ~GoodNode() { --alive; }
};
int GoodNode::alive = 0;

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
    // ⚠️ 注意: w.use_count() 查的也是【强】计数(和 s.use_count() 一样)!
    //         标准【没有】查询弱计数的接口 —— 弱计数只在控制块内部记账(决定控制块何时销毁)
    printf("  观察: s.use_count()=%ld, w.use_count()=%ld (两者相同, 都是强计数)\n",
           s.use_count(), w.use_count());
    if (auto sp = w.lock()) printf("  lock 成功: 对象活着\n");
    s.reset();                                  // 对象销毁
    printf("  对象已销毁, 再 lock: %s\n", w.lock() ? "成功" : "失败(返回空)");

    // ---- §C06.2.1 make_unique 两个易混点 ----
    printf("== make_unique 易混点 ==\n");
    auto p   = std::make_unique<int>(100);     // 单个 int, 值是 100
    auto arr = std::make_unique<int[]>(100);   // 100 个 int 的数组(全部清零)
    printf("  make_unique<int>(100):   *p=%d (一个int值100)\n", *p);
    printf("  make_unique<int[]>(100): arr[0]=%d arr[99]=%d (100个元素全零)\n",
           arr[0], arr[99]);
    printf("  sizeof: unique=%zu(=裸指针,无控制块) shared=%zu(对象+控制块两根)\n",
           sizeof(p), sizeof(s));

    // ---- §C06.4.1 循环引用泄漏 vs weak_ptr 解救 ----
    printf("== 循环引用实验 ==\n");
    {
        auto n1 = std::make_shared<BadNode>("A");
        auto n2 = std::make_shared<BadNode>("B");
        n1->peer = n2;  n2->peer = n1;         // shared 互指
    }   // 外部把手没了, 但互指让计数停在 1 → 永不析构
    printf("  shared 互指出作用域后: alive=%d (泄漏! 本该 0)\n", BadNode::alive);
    {
        auto m1 = std::make_shared<GoodNode>("A");
        auto m2 = std::make_shared<GoodNode>("B");
        m1->peer = m2;  m2->peer = m1;         // weak 互指(不断计数)
        if (auto locked = m2->peer.lock())     // lock: 活着借一个 shared
            printf("  循环内 lock() 成功看到 %s\n", locked->name.c_str());
    }
    printf("  weak 互指出作用域后:   alive=%d (正确析构)\n", GoodNode::alive);

    // ---- §C06.5 直构 vs make_*: 数堆分配次数(重载 operator new 计数, 函数在上方) ----
    printf("== 直构 vs make_* 堆分配计数 ==\n");
    allocs = 0;
    { std::shared_ptr<Res> s(new Res(88)); }
    printf("  直构 shared_ptr: %d 次\n", allocs);
    allocs = 0;
    { auto s = std::make_shared<Res>(89); }
    printf("  make_shared:     %d 次 (对象+控制块打包成一块)\n", allocs);

    // ---- 实战: 自定义删除器管"非 new 资源"(CUDA 语境的前置) ----
    printf("== 自定义删除器 ==\n");
    std::shared_ptr<FILE> file(fopen("/tmp/c06_demo.txt", "w"), [](FILE* f) {
        if (f) { fclose(f); printf("  [deleter] 文件已自动关闭 (RAII!)\n"); }
    });
    if (file) fprintf(file.get(), "hello\n");
    // 离开作用域自动 fclose —— 忘 fclose 导致的句柄泄漏从此不可能
    return 0;
}
