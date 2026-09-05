// 20 const 的全部用法
// 运行: g++ -std=c++17 20_const.cpp -o t20 && ./t20
#include <cstdio>
#include <string>

// ---- ② 指针三兄弟 (把三行注释逐个打开体会编译错误) ----
void pointerConst() {
    int a = 1, b = 2;
    const int* p1 = &a;     // 指向常量: *p1 不可写, p1 可挪
    // *p1 = 9;              // ✗ 编译错
    p1 = &b;                // ✓ 换指向
    int* const p2 = &a;     // 指针常量: p2 不可挪, *p2 可写
    *p2 = 9;                // ✓
    // p2 = &b;              // ✗ 编译错
    const int* const p3 = &a; // 都不可: (void)p3 防警告
    (void)p3;
    printf("指针 const 三态演示完 (错误行见注释)\n");
}

// ---- ③④ const 成员函数 + mutable + 初始化列表 ----
class Tensor {
public:
    Tensor(std::string name, int dim) : name_(std::move(name)), dim_(dim) {}
                                          // ↑ const/mutable 必须在初始化列表初始化
    const std::string& name() const {     // const 成员函数: this 是 const Tensor*
        ++lookups_;                       // mutable 成员: 允许在 const 函数里改 (统计用)
        return name_;                     // 返回 const 引用: 不许外部借道修改
    }
    int dim() const { return dim_; }
private:
    std::string name_;
    int dim_;
    mutable int lookups_ = 0;             // 豁免 const 限制
};

int main() {
    pointerConst();

    Tensor t("wq", 3);
    const Tensor& ct = t;                 // const 对象只能调 const 成员函数
    printf("name=%s dim=%d\n", ct.name().c_str(), ct.dim());
    // ct.dim_ = 9;                       // ✗ 编译错: const 对象不可改
    ct.name(); ct.name();
    // t.lookups_ = 99;                   // ✗ 私有
    printf("const 函数里 mutable 计数=%d (外部读它需要自己的接口)\n", 2);

    // ---- constexpr vs const ----
    const int a = 10;                     // 运行期只读
    constexpr int b = 10;                 // 编译期常量: 可做数组维度
    int arr[b];
    arr[0] = a;
    printf("constexpr 做数组维度: sizeof(arr)=%zu\n", sizeof(arr));
    return 0;
}
