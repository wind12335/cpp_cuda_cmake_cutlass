// 04_shared_ptr_impl.cpp — 手搓 shared_ptr（四件套版·新手注释加强版）
// ═══════════════════════════════════════════════════════════════════════
// md 名词 → 代码实体对照（先扫一眼，读完代码回来对）:
//   "一个裸指针 T*"          → MySharedPtr 成员 p_
//   "指向堆上控制块的指针"     → MySharedPtr 成员 cb_
//   "堆上控制块(账本)"        → struct CtrlBlock 整体（new 在堆上）
//   "强引用计数"              → CtrlBlock::strong（atomic<long>）
//   "弱引用计数"              → CtrlBlock::weak（atomic<long>，是个【数字】，不是 weak_ptr）
//   "deleter 删除器"          → CtrlBlock::del（"怎么销毁对象"的函数指针，默认 delete）
//   "allocator 分配器"        → 教学版省略（"账本内存用什么方式分配"的策略，默认 new）
//
// 编译运行: g++ -std=c++17 -O0 04_shared_ptr_impl.cpp -o t4 && ./t4
// ═══════════════════════════════════════════════════════════════════════
#include <cstdio>
#include <memory>
#include <atomic>

// ────────────────────────────────────────────────────────────────
// 【知识点①】std::atomic<T> 是什么？
//   它是一个"包装类"：把一个普通变量包起来，让它的一切操作变成"不可分割"。
//   atomic<long> 和 std::array 长得像是因为都用了模板尖括号，但它【不是数组】，
//   里面只包着一个数字。
//   为什么需要：普通 long 的自增在 CPU 层面是三步（读→加→写回），两个线程交错
//   执行会互相覆盖（丢更新）；atomic 把三步合成一条不可打断的 CPU 指令
//   （x86 上是 LOCK XADD），多少线程同时加减都不会算错。
//   它的常用成员函数（全是标准库自带的，不用我们写）：
//     .load()        原子地"读"出当前值
//     .store(v)      原子地"写"入 v
//     .fetch_add(1)  原子地 +1，返回【加之前的旧值】
//     .fetch_sub(1)  原子地 -1，返回【减之前的旧值】
//   详见 T2 第 22 题。
// ────────────────────────────────────────────────────────────────

// ══════════════ 第 1 块: 控制块（md 说的"堆上账本"） ══════════════
template <class T>
struct CtrlBlock {
    // 【小知识】struct 的成员默认 public，class 的成员默认 private——
    // 这里用 struct 就是为了让 MySharedPtr / MyWeakPtr 直接读写账本内部。

    std::atomic<long> strong{1};  // 强计数: 有几个 shared_ptr 在"拥有"对象。{1} = 创建时给初始值 1
    std::atomic<long> weak{0};    // 弱计数: 有几个 weak_ptr 在"观察"。
                                  //   ⚠ 它是个【数字】！weak_ptr 是拿着这个数字的【观察者对象】，
                                  //   一个 weak_ptr 观察时它 +1，观察者析构时 -1。

    void (*del)(T*) = nullptr;    // 【删除器】一个函数指针：存"这个对象该怎么销毁"。
                                  //   默认指向 default_delete（就是 delete p）；
                                  //   也可以指向 fclose / cudaFree / 你的自定义函数。
    T* ptr;                       // 对象指针在账本里也存一份（销毁对象时要用）

    // 【问题②】struct 和 class 一样可以写构造函数，这就是 CtrlBlock 的构造函数。
    // explicit 的作用：禁止"T* 隐式转换成 CtrlBlock"这种意外（第 27 题），纯防御写法。
    // ": del(d), ptr(p)" 是初始化列表——在构造函数体执行【前】给成员赋初值（第 20 题）。
    explicit CtrlBlock(T* p, void (*d)(T*)) : del(d), ptr(p) {}

    ~CtrlBlock() { printf("      [账本] 控制块也被释放了 (strong=0 且 weak=0)\n"); }
};

// 默认删除器: md 里"默认是 delete p"说的就是它。你也可以传 fclose/cudaFree 等
template <class T>
static void default_delete(T* p) { delete p; }

// ══════════════ 第 2 块: weak_ptr（观察者） ══════════════
template <class T>
class MySharedPtr;   // 前置声明: 下面 weak_ptr 的函数签名里要用到它，正式定义在后面

template <class T>
class MyWeakPtr {
    // 【问题③】class 的成员默认就是 private——这两行没写 public: 所以是私有的，
    // 外部代码不能直接碰，只能通过下面的公有函数(lock/weak_count)操作。这叫封装。
    T* p_ = nullptr;              // 记着对象的地址（但不保证对象还活着！）
    CtrlBlock<T>* cb_ = nullptr;  // 记着账本的地址——账本活得比对象久（第 5 幕），用来查"对象死了没"

public:
    // 【问题⑤ 前半】构造观察者: 弱计数 +1，强计数【完全不动】——这就是"观察不拥有"
    MyWeakPtr(const MySharedPtr<T>& s);

    ~MyWeakPtr();   // 观察者析构: 弱计数 -1

    // 【问题⑤ 后半】lock() 和"创建观察者"是两个不同时刻的动作，千万别混：
    //   创建观察者（weak+1, strong 不动）→ 发生在 MyWeakPtr 构造时（一次性）
    //   lock()（strong 临时 +1）          → 发生在你【某天想真正用对象】的时候
    //   流程: 对象还活着 → strong+1，换发一个临时的 shared_ptr 给你（它析构时自动 strong-1）
    //         对象已死   → 返回一个【空的】shared_ptr（绝不碰悬垂内存）
    //   所以对 weak_ptr 一生而言: strong 的净变化是 0 —— "weak 不加计数"说的就是净效果。
    MySharedPtr<T> lock() {
        if (cb_ && cb_->strong.load() > 0) {      // 【问题④】load() = atomic 的"原子读"成员函数
            ++cb_->strong;                        // 升格成功 = 临时成为拥有者
            return MySharedPtr<T>(p_, cb_);
        }
        return MySharedPtr<T>();                  // 对象已死 → 空
    }

    // 【问题⑤ 补充】use_weak/weak_count 只是调试用的查询函数: 返回【弱计数的数字】
    // （有几个观察者），不是返回控制块——控制块(cb_)是私有的观察者内部记录。
    long weak_count() const { return cb_ ? cb_->weak.load() : -1; }

    template <class> friend class MySharedPtr;    // 允许 shared_ptr 访问我的私有成员（互信）
};

// ══════════════ 第 3 块: shared_ptr 本体 ══════════════
// md 说的"shared_ptr = 两根指针"就是这两个成员——整个类 sizeof = 16 字节（x64）
template <class T>
class MySharedPtr {
    T* p_ = nullptr;             // 第 1 根指针: 指向管理的对象（std 版里叫 _M_ptr）
    CtrlBlock<T>* cb_ = nullptr; // 第 2 根指针: 指向堆上的控制块（std 版里叫 _M_refcount）

public:
    MySharedPtr() = default;     // 空的 shared_ptr（std::shared_ptr<Res> sp; 这种）

    // 【问题⑥-①】主构造函数: 你 new 好对象交给它管 → 创建账本，强=1。
    // "= &default_delete<T>" 是【默认参数】（第 14 题知识点）：不传删除器就自动用默认的。
    // explicit: 禁止 "MySharedPtr sp = new Res;" 这种把裸指针悄悄变成 shared_ptr 的隐式转换。
    explicit MySharedPtr(T* p, void (*d)(T*) = &default_delete<T>)
        : p_(p), cb_(new CtrlBlock<T>(p, d)) {
        printf("      [创建] 账本上线: strong=1, weak=0\n");
    }

    // 【问题⑥-②】内部构造函数: 只给 weak_ptr::lock() 用！
    //   因为 lock() 在调它【之前】已经把强计数 +1 加好了，这个构造只负责抄两根指针，
    //   【绝不能再加一次】——这就是为什么它和上面的主构造是两个不同的函数。
    MySharedPtr(T* p, CtrlBlock<T>* cb) : p_(p), cb_(cb) {}

    // 【问题⑥-③】拷贝构造函数: "承接"已有的 shared_ptr。
    //   什么时候被调？MySharedPtr b = a;  / 按值传参 / 返回值（RVO 不适用时）。
    //   做的事: 两根指针抄一份 + 强计数 +1 —— 共享的全部实现就这么点。
    MySharedPtr(const MySharedPtr& o) : p_(o.p_), cb_(o.cb_) {
        if (cb_) {
            // 【问题⑥-④】fetch_add(1) = atomic 自带的"原子加 1"成员函数（标准库实现，
            // 底层是 LOCK XADD 指令），返回【加之前的旧值】——这里用不上返回值，忽略。
            cb_->strong.fetch_add(1);
            printf("      [拷贝] strong -> %ld\n", cb_->strong.load());
        }
    }

    // 【问题⑦】析构函数: 每次 shared_ptr 消亡，强计数 -1。
    //   fetch_sub(1) = atomic 自带的"原子减 1"，返回【减之前的旧值】。
    //   旧值 == 1 → 减完是 0 → 我是最后一个拥有者 → 由我负责收尾。
    //   【两个 delete 的区别】删的是两个不同的东西（当初也是两次 new 出来的）：
    //     cb_->del(cb_->ptr) → 删【对象】（走你给的删除器，默认 delete，触发 Res 的析构）
    //     delete cb_          → 删【账本】（控制块结构体本身）
    //   为什么分两步：对象死后 weak_ptr 还需要账本查"对象死了没"，
    //   所以对象死了账本要留；等弱计数也归零，才轮到删账本。
    ~MySharedPtr() {
        if (!cb_) return;
        if (cb_->strong.fetch_sub(1) == 1) {
            printf("      [销毁] strong -> 0, 我是最后一个拥有者, 调用 deleter 销毁对象\n");
            cb_->del(cb_->ptr);              // 第一步: 删对象
            cb_->ptr = nullptr;
            if (cb_->weak.load() == 0)
                delete cb_;                  // 第二步: 没人观察了 → 连账本一起删
            else
                printf("      [账本] 还有 %ld 个观察者, 账本留着给他们查\"对象死了没\"\n",
                       cb_->weak.load());
        }
    }

    void reset() { MySharedPtr().swap(*this); }   // 置空 = 放弃所有权
    void swap(MySharedPtr& o) { T* tp = p_; p_ = o.p_; o.p_ = tp;
                                CtrlBlock<T>* tc = cb_; cb_ = o.cb_; o.cb_ = tc; }
    long use_count() const { return cb_ ? cb_->strong.load() : 0; }   // 查强计数
    long weak_count() const { return cb_ ? cb_->weak.load() : 0; }    // 查弱计数
    T* get() const { return p_; }
    T& operator*() const { return *p_; }
    explicit operator bool() const { return p_ != nullptr; }  // 27 题伏笔: 只许在 if 里当 bool 用

    friend class MyWeakPtr<T>;   // 互信声明: 允许 MyWeakPtr 访问我的私有成员 p_/cb_
};

// weak_ptr 的构造/析构实现（放在类外因为要用 MySharedPtr 的完整定义）
template <class T>
MyWeakPtr<T>::MyWeakPtr(const MySharedPtr<T>& s) : p_(s.p_), cb_(s.cb_) {
    if (cb_) {
        cb_->weak.fetch_add(1);   // 只加弱计数！
        printf("      [观察] weak -> %ld (强计数不变: %ld)\n",
               cb_->weak.load(), cb_->strong.load());
    }
}
template <class T>
MyWeakPtr<T>::~MyWeakPtr() {
    if (!cb_) return;
    // 弱计数减到 0、且对象早已死（强=0）→ 我是最后离开的，账本由我销毁
    if (cb_->weak.fetch_sub(1) == 1 && cb_->strong.load() == 0) {
        delete cb_;
    }
}

// ══════════════ 第 4 块: 演示剧情 ══════════════
struct Res {
    ~Res() { printf("      [对象] Res 析构 (deleter 调用的就是我)\n"); }
};

// 自定义删除器: "怎么销毁"由你定 —— 管文件就 fclose, 管 GPU 显存就 cudaFree
static void my_custom_delete(Res* p) {
    printf("      [deleter] 走的是自定义删除器 (不是默认 delete)!\n");
    delete p;
}

int main() {
    printf("sizeof(MySharedPtr<Res>) = %zu  ← 就是两根指针, 16 字节\n",
           sizeof(MySharedPtr<Res>));
    printf("sizeof(std::shared_ptr<Res>) = %zu  ← std 版也是 16 字节, 同样的两根指针\n\n",
           sizeof(std::shared_ptr<Res>));

    printf("=== 剧情 1: 创建 + 拷贝 ===\n");
    MySharedPtr<Res> a(new Res, &my_custom_delete);   // 强=1, 弱=0, deleter=自定义
    {
        MySharedPtr<Res> b = a;                       // 拷贝构造: 强=2 (账本不新建!)
    }                                                 // b 析构: 强=1
    printf("  现在强=%ld 弱=%ld\n\n", a.use_count(), a.weak_count());

    printf("=== 剧情 2: weak_ptr 观察但不拥有 ===\n");
    MyWeakPtr<Res> w = a;                             // 弱=1, 强不动
    printf("  现在强=%ld 弱=%ld\n", a.use_count(), a.weak_count());
    if (auto locked = w.lock()) printf("  lock() 成功: 对象活着, 拿到了\n");

    printf("\n=== 剧情 3: 最后一个拥有者消失 → 对象死, 但账本留着 ===\n");
    a.reset();                                        // 强=0 → 删对象; 弱=1 → 账本留
    printf("  a 已 reset。此时强=%ld\n", a.use_count());

    printf("\n=== 剧情 4: 对象死后 weak_ptr::lock() 返回空 ===\n");
    if (auto locked = w.lock(); !locked) printf("  lock() 失败: 对象已死, 返回空 (没碰悬垂内存!)\n");

    printf("\n=== 剧情 5: 最后一个观察者也走了 → 账本才退休 ===\n");
    {
        MyWeakPtr<Res> w2 = w;                        // 弱=2
    }                                                 // 弱=1
    printf("  (w 在 main 结束时析构, 之后你会看到账本释放的打印)\n");
    return 0;
}
