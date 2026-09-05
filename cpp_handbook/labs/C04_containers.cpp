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
    std::transform(v.begin(), v.end(), v.begin(), [](int x) { return x * 2; });
    printf("transform 翻倍: "); for (int e : v) printf("%d ", e); printf("\n");

    // ---- map: 词频统计小例 ----
    std::vector<std::string> words = {"gpu", "cpu", "gpu", "gpu", "cpu"};
    std::map<std::string, int> freq;
    for (auto& w : words) freq[w]++;               // [] 不存在时自动插入 0 再自增
    for (auto& [k, c] : freq) printf("词频 %s = %d\n", k.c_str(), c);  // C++17 结构化绑定

    // ---- priority_queue: TopK 思路 ----
    std::priority_queue<int> pq;
    for (int e : {3, 9, 1, 7}) pq.push(e);
    printf("优先队列 top=%d (最大值恒在顶)\n", pq.top());
    return 0;
}
