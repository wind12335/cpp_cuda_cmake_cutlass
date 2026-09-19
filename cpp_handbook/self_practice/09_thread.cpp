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

    std::thread t2([&]{for(y ; y>95; y--){std::cout<<"y:"<<y<<std::endl;}});  // y 走 lambda([&]捕获)
    t1.join();
    t2.join();
    std::cout << "最终 x=" << x << " y=" << y << std::endl;
}

int main(){
    test_thread();
}
