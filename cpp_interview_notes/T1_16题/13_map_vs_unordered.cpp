// 13 map vs unordered_map
// 运行: g++ -std=c++17 -O2 13_map_vs_unordered.cpp -o t13 && ./t13
#include <cstdio>
#include <map>
#include <unordered_map>
#include <string>
#include <chrono>
#include <random>

int main() {
    // ---- 1. 遍历顺序: 有序 vs 哈希序 ----
    std::map<int, std::string> m;
    std::unordered_map<int, std::string> um;
    for (int k : {50, 20, 90, 10, 70}) {
        m[k] = "v"; um[k] = "v";
    }
    printf("map 遍历(升序): ");
    for (auto& [k, v] : m) printf("%d ", k);
    printf("\nunordered 遍历(哈希序): ");
    for (auto& [k, v] : um) printf("%d ", k);
    printf("\n");

    // ---- 2. 点查性能: 10^6 次 (构建完只查, 无 rehash 干扰) ----
    std::mt19937 rng(42);
    std::unordered_map<int, int> big_um;
    std::map<int, int> big_m;
    for (int i = 0; i < 100000; ++i) { big_um[i] = i; big_m[i] = i; }
    std::vector<int> queries(1000000);
    for (auto& q : queries) q = rng() % 100000;

    auto t0 = std::chrono::steady_clock::now();
    volatile long sink = 0;                       // volatile 防止查找被优化掉
    for (int q : queries) { auto it = big_m.find(q); if (it != big_m.end()) sink += it->second; }
    auto t1 = std::chrono::steady_clock::now();
    for (int q : queries) { auto it = big_um.find(q); if (it != big_um.end()) sink += it->second; }
    auto t2 = std::chrono::steady_clock::now();
    printf("map        100万次点查: %lld ms\n",
           (long long)std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());
    printf("unordered  100万次点查: %lld ms\n",
           (long long)std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count());

    // ---- 3. 迭代器稳定性 ----
    auto it = m.begin();
    m[30] = "插入新 key";                          // 树插入不移动已有节点
    printf("map: 插入新元素后旧迭代器仍有效, *it=%d:%s\n", it->first, it->second.c_str());
    // unordered_map 插入可能触发 rehash → 全部迭代器失效(此处不演示 UB)
    printf("(void)sink=%ld\n", (long)sink);
    return 0;
}
