// 12 push_back vs emplace_back
// 运行: g++ -std=c++17 12_push_vs_emplace.cpp -o t12 && ./t12
#include <cstdio>
#include <vector>
#include <string>

class Job {
public:
    Job(std::string name, int prio) : name_(std::move(name)), prio_(prio) {
        printf("  构造(%s,%d)\n", name_.c_str(), prio_);
    }
    Job(const Job& o) : name_(o.name_), prio_(o.prio_) { printf("  拷贝构造(%s)\n", name_.c_str()); }
    Job(Job&& o) noexcept : name_(std::move(o.name_)), prio_(o.prio_) { printf("  移动构造(%s)\n", name_.c_str()); }
private:
    std::string name_;
    int prio_;
};

int main() {
    printf("--- push_back(临时对象): 构造 1 次 + 移动 1 次 ---\n");
    {
        std::vector<Job> v; v.reserve(4);
        v.push_back(Job("gemm", 3));       // 先在调用方构造, 再移动进容器
    }

    printf("--- emplace_back(参数): 只构造 1 次, 就地施工 ---\n");
    {
        std::vector<Job> v; v.reserve(4);
        v.emplace_back("gemm", 3);         // 参数直接转发给 Job(string,int) 在容器内存上构造
    }

    printf("--- 已有左值对象: 两者等价, 都只能拷贝/移动 ---\n");
    {
        std::vector<Job> v; v.reserve(4);
        Job j("cached", 1);
        v.push_back(j);                    // 拷贝(左值)
        v.push_back(std::move(j));         // 移动(转成右值) —— emplace_back 写法也一样
    }
    return 0;
}
