#include<thread>
#include<mutex>
#include<atomic>

#include<iostream>


void test_thread(){
    int x = 0;
    int y = 100;

    // ── 报错原因: 'x' is not captured ──
    // lambda 的 [] 是【捕获列表】(C02 §C02.7): 声明"我要带哪些外面的变量进来"
    //   []  = 空列表 = "外面的东西我什么都不要" → 里面写 x++ 时, 编译器:
    //         "你没申请带 x, 凭什么用它?" → 直接拒绝编译
    //   [&] = 按引用捕获 = "把外面的 x 本尊牵进来" → x++ 改的就是外面那个 x ✓
    //   [=] = 按值捕获 = "复印一份 x 进来" → 改的是复印件, 外面 x 纹丝不动(这里用错!)
    // 线程要改的就是外面的 x 本尊, 所以要 [&]
    // 顺带: std::thread t1 = std::thread(...) 右边那个显式构造是多余的(能编,靠移动),
    //       直接 std::thread t1(...) 即可 —— 下面保留你的写法, 只是标一下
    std::thread  t1 = std::thread([&]{x++;});   // [&]: 线程里改外面的 x 本尊
    std::thread  t2 = std::thread([&]{y--;});   // 同理 y
    t1.join();
    t2.join();
    // 提示: join 之后在下面加一行 std::cout << x << " " << y;
    //      (iostream 你已包含) 就能看到 1 99 —— 不加的话跑了也看不到效果
}

int main(){

    test_thread();
}