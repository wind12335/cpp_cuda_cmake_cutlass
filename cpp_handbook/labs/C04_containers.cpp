// C04 实验: 容器与算法
// 运行: g++ -std=c++17 -O2 C04_containers.cpp -o c04 && ./c04
#include <cstdio>
#include <string>
#include <vector>
#include <map>
#include <queue>
#include <algorithm>
#include <numeric>

int main() {
    // ---- string ----
    std::string s = "hello";
    s += " cuda";
    printf("string: \"%s\", 长度=%zu, 找到 cuda? %s\n", s.c_str(), s.size(),
           s.find("cuda") != std::string::npos ? "是" : "否");

    // ---- vector: 模拟"CUDA host 端准备数据"的标准流程 ----
    std::vector<float> host(8);
    for (size_t i = 0; i < host.size(); ++i) host[i] = (float)i;   // 填数据
    float sum = std::accumulate(host.begin(), host.end(), 0.0f);
    printf("vector 8 个 float, 和=%.1f (accumulate 就是 CPU 版 reduce)\n", sum);

    // ---- 算法六连 ----
    std::vector<int> v = {5, 3, 8, 1};
    std::sort(v.begin(), v.end());
    printf("sort 后: ");
    for (int e : v) printf("%d ", e);
    printf("\n");
    printf("max=%d, >3 的个数=%d\n",
           *std::max_element(v.begin(), v.end()),
           (int)std::count_if(v.begin(), v.end(), [](int x) { return x > 3; }));

    // ---- §C04.6.1 transform: 三问(读哪段?写哪去?怎么变?) 两种用法对照 ----
    // 用法A: 原地翻倍(第3参数写回 v 自己)
    std::transform(v.begin(), v.end(), v.begin(), [](int x) { return x * 2; });
    printf("transform 原地翻倍:  "); for (int e : v) printf("%d ", e); printf("\n");
    // ↑ 等价于 for(i) v[i] = v[i]*2;

    // 用法B: 写到另一个 vector(v 原样不动) —— 注意 w 必须提前 resize, transform 不扩容
    std::vector<int> w(v.size());                     // 先腾好一样大的空间
    std::transform(v.begin(), v.end(), w.begin(),     // 第3参数改成 w 的开头 = 结果写进 w
                   [](int x) { return x < 0 ? 0 : x; });  // 负数变0, 其他不变
    printf("transform 写到 w(非负化): "); for (int e : w) printf("%d ", e); printf("\n");
    printf("v 没被动过:     "); for (int e : v) printf("%d ", e); printf("\n");

    // ---- §C04.2.1 push_back vs emplace_back: 装 pair 才看得出区别 ----
    std::vector<std::pair<std::string, int>> vp;
    vp.push_back(std::make_pair("gpu", 8));  // 先造临时 pair, 再拷贝进 vp (一次构造+一次拷贝)
    vp.emplace_back("cpu", 16);              // 原料直接进 vp 原地构造 (只一次构造, 零拷贝)
    // 两者结果完全一样, 打印分不出来; 区别只在中间过程省了临时对象和拷贝
    for (auto& [name, n] : vp) printf("设备 %s x%d\n", name.c_str(), n);

    // ---- map: 词频统计小例 ----
    std::vector<std::string> words = {"gpu", "cpu", "gpu", "gpu", "cpu"};
    std::map<std::string, int> freq;
    for (auto& w : words) freq[w]++;               // [] 不存在时自动插入 0 再自增

    // §C04.3.1 结构化绑定分三步看:
    // (1) freq 里每个元素其实是个 pair<string,int> 小盒子
    // (2) 老写法: 每圈拿到一个 pair, 用 .first/.second 取零件(名字没含义, 难读)
    for (auto& p : freq) printf("[老写法] 词频 %s = %d\n", p.first.c_str(), p.second);
    // (3) 新写法(C++17): [k, c] 接到盒子的同时拆开, 给两个零件起有意义的名字
    for (auto& [k, c] : freq) printf("[新写法] 词频 %s = %d\n", k.c_str(), c);

    // ---- priority_queue: TopK 思路 ----
    std::priority_queue<int> pq;
    for (int e : {3, 9, 1, 7}) pq.push(e);
    printf("优先队列 top=%d (最大值恒在顶)\n", pq.top());
    return 0;
}
