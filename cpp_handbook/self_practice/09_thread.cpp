#include<thread>
#include<mutex>
#include<atomic>
#include<functional>   // std::ref 的家
#include<iostream>

// ── 坑①: 你原来把 x_add 写在 test_thread【里面】——
//    报错: error: a function-definition is not allowed here before '{' token
//    原因: C++ 禁止"函数体内再定义函数"(嵌套函数不存在)。函数只能定义在
//    文件作用域(像现在这样)或类里面。lambda 是唯一能"在函数里写函数体"的东西。
void x_add(int& x){                    // 外置函数碰外面的数据, 只能靠参数传引用
    for( ; x<200; x++){                // (普通函数没有捕获列表! 那是 lambda 独有的)
        std::cout<<"x:"<<x<<std::endl;
    }
}

void test_thread(){
    int x = 0;                         // 家在 main 线程栈的栈帧里(§C09.1.2)
    int y = 100;

    // ── 坑②: std::thread 传参【默认拷贝一份】再传给函数 ──
    //    就算函数签名是 int&, 直接写 x 也会编译错(实测):
    //    "std::thread arguments must be invocable after conversion to rvalues"
    //    解法: std::ref(x) = "把 x 的引用打包, 告诉 thread 别拷贝, 传本尊"
    std::thread t1(x_add, std::ref(x));            // x 走外置函数(引用参数)

    std::thread t2([&]{for(y ; y>-5; y--){std::cout<<"y:"<<y<<std::endl;}});  // y 走 lambda([&]捕获)
    t1.join();
    t2.join();
    // ── 修复: 原来写成 std::thread(std::this_thread::get_id()) ──
    //   那是在"用 id 构造一个新线程对象"(thread 的构造函数要的是【可调用的函数】,
    //   thread::id 不是函数) → 编译错 no matching function for call to 'std::thread(...)'
    // 正确写法: cout 能直接打印 thread::id (它支持 <<; printf 才需要先 hash 成数字)
    // 输出的是【谁】? = 此刻正在执行这行代码的线程 = main!
    //   (join 之后回到 main 线程继续跑; t1/t2 已经结束被回收, 不是它们的 id)
    // 想要 t1/t2 的 id: ① 在 x_add/lambda【体内】调 this_thread::get_id()
    //                   ② 或 join 之前在外面调 t1.get_id()
    std::cout<< "main线程id: " << std::this_thread::get_id() << std::endl;
    std::cout << "最终 x=" << x << " y=" << y << std::endl;
}

void test_mutexthread(){
    std::mutex<std::thread> mt;
    std::thread t1();
    std::thread t2();
}
int main(){
    test_thread();
}
