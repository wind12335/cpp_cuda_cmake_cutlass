// 手搓 shared_ptr —— 修订版(你的结构保留, 修了6个bug + deleter连线)
// 运行: g++ -std=c++17 -Wall 06b_shoucuosharedptr.cpp -o 06b && ./06b
#include <cstdio>
#include <iostream>
using std::cout;
using std::endl;

// ⚠️bug⑥: 被用的类型要先定义 —— Deleter 和 controlBlock 都搬到模板类【前面】
// ── deleter: 模板一份就够, 不用 int/float/char 三个重载 ──
template <typename T>
struct DefaultDeleter {                 // 默认删除器: 普通 new 出来的对象
    void operator()(T* p) const {       // operator() 让 struct 变成"可调用的东西"
        delete p;                       //   (X03 的 functor 就是这个)
    }
};

struct controlBlock {
    int _strong = 0;    
    int _weak = 0;      //   弱计数阶段4才用, 先放着
};

template <typename T, typename Deleter = DefaultDeleter<T>>   // ← deleter 连线①:
class shared_ptr {                                             //   做成第二个模板参数,
public:                                                        //   不传就用默认 delete 版
    explicit shared_ptr(T* ptr = nullptr)
        : _ptr(ptr),
          _controller(ptr ? new controlBlock : nullptr) 
    {
        if (_controller) {
            _controller->_strong = ptr ? 1 : 0;
        }
    }


    shared_ptr(const shared_ptr& other) :  // 拷贝 强引用+1
        _ptr(other._ptr), _controller(other._controller) {
        if (_controller) ++(_controller->_strong);   // 防空: 从空 shared_ptr 拷贝不崩
    }

    shared_ptr(shared_ptr&& other) noexcept :  // 移动: 偷完置空, 不动计数
        _ptr(other._ptr), _controller(other._controller) {
        other._ptr = nullptr;
        other._controller = nullptr;
    }

    shared_ptr& operator=(const shared_ptr& other) {  // 拷贝赋值
        if (this != &other) {                // ⚠️bug④: 用 this != &other(比地址),
            if (_controller && --(_controller->_strong) == 0) {  //   *this != other 没定义且语义不对
                _deleter(_ptr);              // ← deleter 连线②: 原来裸写的 delete _ptr 换成这个
                delete _controller;
                cout << "拷贝赋值, 原本的引用归0, 触发删除" << endl;
            }
            _ptr = other._ptr;
            _controller = other._controller;
            if (_controller) ++(_controller->_strong);   // 防空: 赋成空的不崩
        }
        return *this;
    }

    shared_ptr& operator=(shared_ptr&& other) noexcept {  // ← 补 noexcept!
        // (实测四场景全过: 不同账本/自移动/同账本/从空移动 —— 逻辑你已写对)
        if (this != &other) {
            if (_controller && --(_controller->_strong) == 0) {
                _deleter(_ptr);
                delete _controller;
                cout << "移动赋值, 原本的引用归0, 触发删除" << endl;
            }
            _ptr = other._ptr;
            _controller = other._controller;
            other._ptr = nullptr;             // 偷完置空
            other._controller = nullptr;
        }
        return *this;
    }

    ~shared_ptr() {
        if (_controller && --(_controller->_strong) == 0) {
            _deleter(_ptr);                  // ← deleter 连线②: 同一处替换
            delete _controller;
            cout << "触发析构" << endl;
        }
    }

    // ⚠️ 原版 cout << *_ptr 遇到空指针直接段错误(被 move 掏空的对象/空对象)!
    void print() const {
        if (_ptr) cout << *_ptr << endl;
        else      cout << "(空)" << endl;
    }
    T& operator*()  const { return *_ptr; }  // 让 *sp 像指针
    T* operator->() const { return _ptr; }   // 让 sp->member 像指针(C03 §C03.1.2)
    T* get()        const { return _ptr; }   // 拿裸指针(FILE 这种不能解引用的东西要用)
    int use_count() const { return _controller ? _controller->_strong : 0; }

private:
    T* _ptr = nullptr;              
    controlBlock* _controller = nullptr;
    Deleter _deleter;               // ← deleter 连线③: 成员放这, 所有副本天然带同一套删法
};

// ══════════════ 验收实验 ══════════════
struct Res {                        // 会报账的资源: 数创建/销毁次数
    int id;
    explicit Res(int i) : id(i) { cout << "  Res(" << id << ") 出生" << endl; }
    ~Res() { cout << "  Res(" << id << ") 销毁" << endl; }
};
struct FileCloser {                 // 自定义 deleter: 证明"删法"可以换!
    void operator()(FILE* f) const {
        if (f) { fclose(f); cout << "  [FileCloser] 文件自动关闭(RAII!)" << endl; }
    }
};



int main() {
    {
        shared_ptr<int> test = shared_ptr<int>(new int(42)); // 创建一个 shared_ptr，指向一个整数
        shared_ptr<int> test2 = test; // 拷贝构造，引用计数增加

        shared_ptr<int> test3 = shared_ptr<int>(new int(33));
        cout << "test3.use_count(): " << test3.use_count() << endl; // 输出引用计数
        test3 = test; // 拷贝赋值，原来的引用计数减少，新的引用计数增加
        cout << "f拷贝赋值后test3.use_count(): " << test3.use_count() << endl; // 输出引用计数

        shared_ptr<int> test4 = shared_ptr<int>(new int(56)); 

        test = test4; // 【其实是拷贝赋值!】test4 是左值 → 走 operator=(const shared_ptr&)
                     // 想走移动赋值必须写 test = std::move(test4);
                     // 两种都合法, 区别: 拷贝后 test4 还有内容(use=2); 移动后 test4 被掏空
        test4.print();
        test.print();
        cout << "test.use_count(): " << test.use_count() << endl; // 输出

    }

    cout << "------------------------ ① 计数对账 --------------------------------" << endl;
    {
        shared_ptr<Res> a(new Res(1));
        cout << "  出生: use=" << a.use_count() << endl;              // 1
        {
            auto b = a;                                               // 拷贝
            cout << "  拷贝后: use=" << a.use_count() << endl;         // 2
        }
        cout << "  b 死后: use=" << a.use_count() << endl;             // 1
    }      
                                                               // a 出作用域 → 销毁



    // cout << "== ② 赋值放旧账 ==" << endl;
    // {
    //     shared_ptr<Res> a(new Res(2)), c(new Res(3));
    //     c = a;                               // c 的旧账(Res3)归0触发删除, 然后共有 Res2
    //     cout << "  赋值后: use=" << a.use_count() << endl;             // 2
    // }





    // cout << "== ③ 自定义 deleter 管 FILE(不是 new 出来的东西!) ==" << endl;
    // {
    //     shared_ptr<FILE, FileCloser> f( fopen("/self_practice/shousuo.txt", "12345678小明、小刚、小红") );  // 用自定义 deleter 管 FILE
    //     if (f.get()) fprintf(f.get(), "手搓成功\n");                   // FILE 用 get() 拿裸指针
    // }    
    
    
    
    // // 出作用域 → FileCloser 自动 fclose
    // cout << "== ④ 空 shared_ptr 不崩 ==" << endl;
    // {
    //     shared_ptr<Res> e;                   // 默认构造: 账本都没开
    //     cout << "  空: use=" << e.use_count() << endl;                 // 0
    //     auto g = e;                          // 从空拷贝
    //     cout << "  从空拷贝: use=" << g.use_count() << endl;           // 0, 没崩 ✓
    // }
    return 0;
}
