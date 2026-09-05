// C02b 实验: static 四种用法逐一实测 —— 讲透"改变默认命运"
// 编译运行: g++ -std=c++17 -O2 -pthread C02b_static.cpp -o c02b && ./c02b
#include <cstdio>
#include <string>

// ───────── 场景 1: 函数内 static 局部变量 = "给函数加记忆" ─────────
int callCount() {
    static int count = 0;       // 只在【第一次调用】时执行这行; 之后每次进来跳过初始化,
    return ++count;             // 直接用上一次留下的值 —— 函数有了"记忆"
}

// 场景 1b: 昂贵的查找表只算一次 (工程常用!)
const char* levelName(int level) {
    static const char* table[] = {"DEBUG", "INFO", "WARN", "ERROR"};   // 第一次调用时初始化, 之后直接查
    return table[level % 4];
}

// 场景 1c: 单例 —— 全局唯一的配置对象, 第一次用到才创建 (Meyers 单例)
struct Config {
    int threadNum = 8;
};
Config& getConfig() {
    static Config inst;         // C++11 保证: 多线程同时第一次调用也只构造一次
    return inst;                // 调用方拿到同一个对象的引用 —— 全工程就这一份配置
}

// ───────── 场景 2+3: 类的 static 成员 ─────────
class Kernel {
public:
    static inline int alive = 0;        // 全类共享一份: 记录"现在活着几个对象"
    static inline int totalCreated = 0; // 历史上创建过几个
    Kernel()  { ++alive; ++totalCreated; }
    ~Kernel() { --alive; }
    static int reportAlive() {          // 场景3: 静态成员函数——没有 this,
        return alive;                   //   只能碰静态成员(不需要任何对象就能调用)
    }
private:
    int local_id = 0;                   // 普通成员: 每个对象各一份
};

// ───────── 场景 4: 全局 static = "本文件的私有工具" ─────────
// 加 static: 这个函数只在【本 .cpp】可见。别的文件即使写同名函数也不会冲突。
// (多人协作防命名冲突; 现代等价写法: 匿名命名空间 namespace { ... })
static int fileLocalHelper(int x) { return x * 2; }

int main() {
    printf("── 场景1a: static 局部 = 函数的记忆 ──\n");
    int c1 = callCount();      // 第 1 次调用
    int c2 = callCount();      // 第 2 次
    int c3 = callCount();      // 第 3 次
    printf("callCount 三次调用: %d, %d, %d  ← 普通局部变量每次都会是 1\n", c1, c2, c3);

    printf("── 场景1b: 贵重查找表只建一次 ──\n");
    printf("levelName(2) = %s\n", levelName(2));
    printf("levelName(3) = %s  (表只在第一次调用时建过)\n", levelName(3));

    printf("── 场景1c: 单例 = 全工程唯一一份配置 ──\n");
    getConfig().threadNum = 64;                        // 一处修改
    printf("另一处读取 threadNum = %d (同一份!)\n", getConfig().threadNum);

    printf("── 场景2: 类 static 成员 = 全类共享 ──\n");
    {
        Kernel a, b, c;
        printf("  3 个对象存活期间, Kernel::alive = %d\n", Kernel::alive);
    }
    printf("  离开作用域全析构后, alive = %d, 历史创建总数 = %d\n",
           Kernel::reportAlive(), Kernel::totalCreated);

    printf("── 场景4: 文件私有函数 ──\n");
    printf("  fileLocalHelper(21) = %d (本文件私有, 不怕和别人撞名)\n", fileLocalHelper(21));

    printf("\n【什么时候别用 static】①多线程同时读写同一个 static 变量要加锁;\n");
    printf("  ②别把 static 当\"全局变量筐\"乱塞状态——状态越多越难排查。\n");
    return 0;
}
