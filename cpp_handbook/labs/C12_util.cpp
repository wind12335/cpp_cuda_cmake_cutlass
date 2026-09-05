// C12 实验: chrono 计时 + random 初始化 + numeric 四件套
// 运行: g++ -std=c++17 -O2 C12_util.cpp -o c12 && ./c12
#include <cstdio>
#include <chrono>
#include <random>
#include <numeric>
#include <vector>
#include <bit>

int main() {
    // ---- chrono: 测一段代码耗时 ----
    using clock_ = std::chrono::steady_clock;
    auto t0 = clock_::now();
    long s = 0;
    for (int i = 0; i < 10'000'000; ++i) s += i;
    auto t1 = clock_::now();
    printf("循环耗时 %.2f ms (sum=%ld)\n",
           std::chrono::duration<double, std::milli>(t1 - t0).count(), s);

    // ---- random: 可复现地初始化权重 ----
    std::mt19937 rng(42);                              // 固定种子 → 每次运行结果一致
    std::normal_distribution<float> gauss(0.0f, 1.0f);
    std::vector<float> w(5);
    for (auto& x : w) x = gauss(rng);
    printf("高斯初始化权重: ");
    for (float x : w) printf("%.3f ", x);
    printf("\n");

    // ---- numeric 四件套 ----
    std::vector<int> v = {1, 2, 3, 4};
    std::iota(v.begin(), v.end(), 1);                  // 1,2,3,4
    printf("sum=%d, 点积(自己)=%d\n",
           (int)std::accumulate(v.begin(), v.end(), 0),
           (int)std::inner_product(v.begin(), v.end(), v.begin(), 0));
    std::vector<int> prefix(v.size());
    std::partial_sum(v.begin(), v.end(), prefix.begin());
    printf("前缀和: "); for (int e : prefix) printf("%d ", e); printf("\n");

    // ---- bit (C++20, 需要 -std=c++20 才启用; 本机 GCC11 支持) ----
#if defined(__cpp_lib_bitops)
    printf("popcount(0b1011)=%u  (1 的个数, C++20)\n", (unsigned)std::popcount(0b1011u));
#else
    printf("(当前 -std 不含 C++20 bit 库, popcount 未启用)\n");
#endif
    return 0;
}
