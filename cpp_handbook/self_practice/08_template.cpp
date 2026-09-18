#include<utility>
#include<iostream>
#include<memory>
#include<string>

// ⚠️ 先说结论: std::forward 和 CRTP 是【两件不相干的事】, 硬缝是缝不上的:
//    CRTP 回答的问题是 "调【哪个类型】的方法" → 工具是 static_cast<D*>(this)
//    forward 回答的问题是 "这个【参数】原来是左值还是右值" → 前提是万能引用 Args&&
//    this 永远是个普通指针(左值), 没有左右值身份可"保真" → forward 在 CRTP 转发里无事可做
//
// ⚠️ 顺带学到一个模板特性: 类模板的成员函数【不调用就不编译】(惰性实例化)——
//    所以 name_impl 里即使写了编不过的代码, 只要没人调用它, 整个文件照样编译通过!
template<typename T>
class Animals {
    public:
        void speak_impl(){
            static_cast<T*>(this)->speak();
        }
        void name_impl(){
            static_cast<T*>(this)->name();    // ← 原来写 std::forward<T>(this) 编译错的
                                              //    报错(调用时才会现形): no matching function
                                              //    for call to 'forward<Dog>(Animals<Dog>*)'
                                              //    —— forward<Dog> 要 Dog&/Dog&&, 你给它指针
        }

        void operator()(){
            static_cast<T*>(this)->operator()();
            // ⚠️ 陷阱备忘: 若派生类忘了定义 operator(), 这里会转回基类的自己 → 无限递归
        }

};

class Dog : public Animals<Dog> {
    public:
        Dog(const std::string& n) : name_(n)  { std::cout << "  狗:名字【拷贝】进来" << std::endl; }
        Dog(std::string&& n)      : name_(std::move(n)) { std::cout << "  狗:名字【移动】进来" << std::endl; }
        void speak(){
            std::cout << "汪汪汪 (" << name_ << ")" << std::endl;
        }
        void name(){
            std::cout << "我是狗" << std::endl;
        }
        void operator()(){
            std::cout << "我是狗" << std::endl;
        }
    private:
        std::string name_;
};


class Cat : public Animals<Cat> {
    public:
        void speak(){
            std::cout << "喵喵喵" << std::endl;
        }
        void name(){
            std::cout << "我是猫" << std::endl;
        }
        void operator()(){
            std::cout << "我是猫" << std::endl;
        }
};

// ═══ forward 的正确用武之地: 工厂函数(make_unique 的内脏就是这几行) ═══
// 情景: 我要"造动物", 但构造参数五花八门(名字/年龄/...) → 参数包 + 万能引用全收,
//       再【原封不动】转给真正的构造函数 —— 原来是右值的保住移动路线!
template<typename T, typename... Args>
std::unique_ptr<T> make_animal(Args&&... args) {         // 万能引用: 左右值都收
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));  // 保真转发
    // 对照: 写成 new T(args...) 会怎样? args 有名字=左值 → 名字永远走【拷贝】进来!
    //       写成 new T(std::move(args)...) 会怎样? 左值也被抢走 → 调用方的名字被掏空!
}

int main(){
    std::cout << "== CRTP + 仿函数 ==" << std::endl;
    Dog dog("旺财");
    dog.name_impl();
    dog();                    // operator(): 基类转发到派生类的仿函数
    Cat cat;
    cat();

    std::cout << "== forward 的优势在工厂函数里显现 ==" << std::endl;
    std::string my_name = "来福";                  // 左值(我还要用)
    auto d1 = make_animal<Dog>(my_name);           // 传左值 → 名字【拷贝】进来, my_name 还能用
    std::cout << "  my_name 还在: " << my_name << std::endl;
    auto d2 = make_animal<Dog>(std::string("小黑")); // 传右值(临时) → 名字【移动】进来, 零拷贝!
    d1->speak();
    d2->speak();
}