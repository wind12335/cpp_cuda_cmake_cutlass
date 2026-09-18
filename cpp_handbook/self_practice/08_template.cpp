#include<utility>
#include<iostream>
#include<memory>
#include<string>
#include<vector>

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
        Animals() = default;
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

        virtual ~Animals() = default;

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

// ═══════════════════════════════════════════════════════════════
// 虚函数版: 同一个"动物"概念, 换成【运行期】分发 —— 和上面 CRTP 版肩并肩对照
// ═══════════════════════════════════════════════════════════════
struct VAnimal {                          // V 前缀 = virtual 版
    virtual void speak() = 0;             // 纯虚函数: 接口契约("想当动物必须有speak")
    virtual void name()  = 0;             // = 0 → 本类是抽象类, 不能 VAnimal a; 直接造
    virtual ~VAnimal() = default;         // ⚠️ 基类析构必须 virtual(面试02题): 
};                                        //    否则 delete VAnimal* 只析构基类部分→派生成员泄漏

struct VDog : VAnimal {                   // override: 让编译器帮你核对签名(手滑写错会报错)
    void speak() override { std::cout << "  汪汪汪(virtual版)" << std::endl; }
    void name()  override { std::cout << "  我是狗(virtual版)" << std::endl; }
};
struct VCat : VAnimal {
    void speak() override { std::cout << "  喵喵喵(virtual版)" << std::endl; }
    void name()  override { std::cout << "  我是猫(virtual版)" << std::endl; }
};

// ── 双接口对照: 两个函数长得几乎一样, 绑定时机天差地别 ──
// A. 模板接口: A 是什么类型, 编译期定死 → 直接调(甚至内联)。Dog/VDog 都能进
template<typename A>
void speak_twice_static(A& a) { a.speak(); a.speak(); }
// B. 虚接口: a->speak() 运行期查 vptr→vtable → 间接跳转。只有 VAnimal 家族能进
void speak_twice_virtual(VAnimal& a) { a.speak(); a.speak(); }

int main(){
    std::cout << "== CRTP + 仿函数(编译期分发) ==" << std::endl;
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

    // ═══ virtual 的独门绝技: 容器混装(CRTP 做不到!) ═══
    //    Animals<Dog> 和 Animals<Cat> 是两个【不相干的类型】→ 放不进同一个 vector;
    //    VDog/VCat 有共同的基类 VAnimal → 都能装进同一个 vector<unique_ptr<VAnimal>>
    std::cout << "== virtual 版: 一个动物园混装狗和猫 ==" << std::endl;
    std::vector<std::unique_ptr<VAnimal>> zoo;
    zoo.push_back(std::make_unique<VDog>());       // 狗
    zoo.push_back(std::make_unique<VCat>());       // 猫 —— 同一个容器!
    zoo.push_back(std::make_unique<VDog>());       // 又一只狗
    for (auto& a : zoo) a->speak();                // 同一行代码, 每个元素运行期各查各的表

    std::cout << "== 双接口对照(同样写 a.speak(), 绑定时机不同) ==" << std::endl;
    std::cout << "模板接口(编译期):" << std::endl;
    speak_twice_static(dog);        // 进的是 CRTP 版 Dog: 编译期就把 Dog::speak 焊死
    speak_twice_static(*zoo[0]);    // 进的是 VDog: 注意!模板版拿具体类型时也会编译期绑定
    std::cout << "虚接口(运行期):" << std::endl;
    speak_twice_virtual(*zoo[0]);   // 通过基类引用进 → 每次调用运行期查 vtable
    speak_twice_virtual(*zoo[1]);
}