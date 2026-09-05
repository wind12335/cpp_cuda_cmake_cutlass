# CUDA 八股 25 题 —— 讲解 + LeetCUDA 实战绑定

与 `cpp_interview_notes/` 配套的 CUDA 部分。每题一个 `.md`：
**60-90 秒面试口述版 → 高频追问 → 动手绑定（指向你本地 LeetCUDA 的具体文件/命令）→ 记忆钩子**。

代码基本不重写——绑定到你已有的 LeetCUDA 仓库；仅 3 个 LeetCUDA 不覆盖的主题
（stream/event 重叠、pinned memory、错误排查）补了 2 个最小 demo（这正是你论文的看家领域）。

## 两条主线

**主线 A：interview/notes-v2.cu（可直接跑，核心锚点）**
你仓库里现成的 4865 行面试 kernel 大合集（warp 规约 → elementwise → softmax/norm → RoPE →
transpose → GEMV → GEMM(MMA/WGMMA) → FlashAttention），已编译好二进制：

```bash
cd ~/workspace/LeetCUDA/kernels/interview
./notes_v2_cute_sm89.bin              # 跑全部 kernel 的正确性验证表
./notes_v2_cute_sm89.bin --bench-all  # 跑性能对比
```

**主线 B：各 kernel 目录（读代码为主，跑需 torch）**
`elementwise/`、`mat-transpose/`、`sgemm/`、`hgemm/`、`flash-attn/`、`swizzle/` 等，
每个目录是 `.cu`（kernel）+ `.py`（torch 测试）。当前环境没装 torch，先**读代码**；
要跑的话装一次：`pip install torch --index-url https://download.pytorch.org/whl/cu128`，
然后 `cd 对应目录 && python xxx.py`。

## 题目总表

### T1_10题 —— 概念必答

| # | 题目 | 主绑定 |
|---|------|--------|
| 01 | GPU 执行模型（SM/warp/SIMT） | notes-v2 Phase 0 + 验证表 |
| 02 | 线程索引与 grid-stride | elementwise.cu f32 vs f32x4 |
| 03 | 内存层级和量级（1/30/200/500） | hgemm naive vs async |
| 04 | 合并访存 | elementwise Vec4 + mat-transpose |
| 05 | bank conflict | mat_transpose.cu + nvidia-nsight/bank_conflicts.md |
| 06 | warp divergence | notes-v2 Phase 1 交错归约 |
| 07 | occupancy | hgemv（低占用）vs elementwise（高占用）|
| 08 | pinned memory | **demos/stream_overlap.cu** |
| 09 | stream + event | **demos/stream_overlap.cu** |
| 10 | launch 失败排查三板斧 | **demos/launch_error.cu** |

### T2_15题 —— 优化套路

| # | 题目 | 主绑定 |
|---|------|--------|
| 11 | tiling 为什么有效 | sgemm.cu 版本演进 + notes-v2 Phase 7 |
| 12 | double buffering / cp.async | sgemm_async → hgemm_mma_stage |
| 13 | register tiling | sgemm thread-tile + hgemm_mma frag |
| 14 | warp shuffle 规约 | notes-v2 Phase 1 + block_all_reduce.cu |
| 15 | reduce 六级阶梯（手写王牌） | notes-v2 Phase 1 + 你的归约优化详解.md |
| 16 | Tensor Core | hgemm/wmma → hgemm/mma/basic |
| 17 | ldmatrix / swizzle | swizzle/ 目录 + print_swizzle_layout.py |
| 18 | FlashAttention 为什么快 | notes-v2 Phase 8 + flash-attn/mma/ 变体 |
| 19 | roofline | notes-v2 Phase 0 + ncu --set roofline |
| 20 | block size 怎么选 | 三种 launch 哲学对照 |
| 21 | fp16/bf16/tf32/fp8 | hgemm_mma + sgemm_wmma_tf32 |
| 22 | kernel 里能用什么 | relu.cu + 亲手造 __syncthreads 死锁 |
| 23 | CUDA Graph | 概念题 + vLLM 源码导读 |
| 24 | 怎么证明优化有效 | notes-v2 --bench-all 的测法纪律 |
| 25 | profiling：nsys→ncu | 实测命令全在文中（工具已装） |

## 使用方法

1. 先跑一遍主线 A 的验证表和 `--bench-all`，对数字建立手感（这一步 5 分钟）。
2. 每天 2-3 题：读 md 口述 → 按绑定读/跑对应 kernel → 合上讲一遍。
3. 08/09/10 三题跑 `demos/` 下两个 demo（编译命令在文件头），它们是你论文领域的微缩模型。
4. 面试前：只刷各 md 的"口述版"+"记忆钩子"，对照 LeetCUDA 代码说例子。
5. `bash check_all.sh` 一键编译检查 demos 与 notes-v2。

## 环境备忘

- GPU：RTX 4060 Laptop（sm_89，24 SM，8GB），CUDA 12.8；`nvcc/ncu/nsys` 均已装。
- 所有编译统一：`nvcc -std=c++17 -O2 -arch=sm_89 xxx.cu -o xxx`
- LeetCUDA 各 kernel 若需运行：装 torch（命令见上），`python xxx.py` 即测。
- WSL 注意：`nsys` 直接可用；`ncu` 需先在 Windows 侧 NVIDIA 控制面板开 GPU 性能计数器权限
  （见 25 题 md 的实测备注）。
- 参考数字（本机 4060 Laptop 实测）：CuTe HGEMM FP16 约 20-24 TFLOPS，为 cuBLAS 的 0.66-0.79x。

## 📖 阅读约定

- 每题末尾的 **"记忆钩子"** 是压缩口诀，只给理解后的人当回忆开关——直接读看不懂是正常的，
  口诀下都配了 **"人话版"**（已全部补齐）：先读正文的"面试口述版"，懂了再用钩子自测。
- 正文遇到没解释的术语 = bug，报文件名+原句，我来补解释。
