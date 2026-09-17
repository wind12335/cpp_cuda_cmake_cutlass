// ============================================================
// PrintOpNames.cpp —— 我的第一个 MLIR Pass（单文件版）
// 【前置标注】
//   需要：C++ 虚函数（cpp_handbook/C02.4）、std::unique_ptr（C02.6 智能指针）
//   不需要：任何 TableGen 知识（官方教程用 tblgen 生成样板，这里手写，
//          为的是让你第一眼看到 Pass 的"骨架"到底长什么样）
//
// 人话版：MLIR 程序在内存里是一棵 Operation 树（一切皆 Operation）。
//   Pass 就是一个"遍历这棵树并动手脚"的函数对象。
//   本文件做三件事：
//     ① 定义 Pass：walk 整棵树，打印每个 Operation 的名字
//     ② 注册 Pass：让它变成 my-opt 的命令行选项 --print-op-names
//     ③ 复用官方 MlirOptMain：我们的 my-opt 天生自带 mlir-opt 全部功能
// ============================================================

#include "mlir/Dialect/Func/IR/FuncOps.h"     // func.func / func.return
#include "mlir/IR/DialectRegistry.h"
#include "mlir/InitAllDialects.h"             // 一次性注册所有官方 dialect
#include "mlir/Pass/Pass.h"                   // PassWrapper 基类
#include "mlir/Pass/PassManager.h"
#include "mlir/Pass/PassRegistry.h"           // PassPipelineRegistration
#include "mlir/Support/LLVM.h"                // mlir::Operation 等别名
#include "mlir/Tools/mlir-opt/MlirOptMain.h"  // 官方工具主体，直接复用
#include "llvm/Support/raw_ostream.h"         // llvm::errs()

using namespace mlir;

// ------------------------------------------------------------
// ① Pass 本体
// ------------------------------------------------------------
// PassWrapper<自己, OperationPass<ModuleOp>> 读法：
//   - OperationPass<ModuleOp>：这是个"函数级以上"的 Pass，
//     MLIR 会把它调度到每个 ModuleOp 上各跑一次
//   - PassWrapper：CRTP（奇异递归模板模式，见 cpp_handbook/C02.4 虚函数篇）
//     帮你把 clone() 之类的样板代码自动生成，免手写
struct PrintOpNamesPass
    : public PassWrapper<PrintOpNamesPass, OperationPass<ModuleOp>> {
  // runOnOperation 是 Pass 的"main 函数"：拿到一个 Operation，
  // 你想检查就检查，想改就改，想报错就 signalPassFailure()
  void runOnOperation() override;
};

void PrintOpNamesPass::runOnOperation() {
  // walk = MLIR 版的"深度优先遍历"，对树上每个 Operation 调一次 lambda。
  // 对照 CUDA：类似 grid-stride loop 遍历所有数据，只不过这里遍历的是 IR 树
  getOperation()->walk([](Operation *op) {
    // op->getName() 拿到的是比如 "func.func"、"arith.addi" 这种名字
    llvm::errs() << "访问到 op: " << op->getName() << "\n";
  });
  // 没调 signalPassFailure() = 这个 Pass 成功结束
}

// 工厂函数：返回堆上的 Pass 对象（unique_ptr 自动防泄漏，见 C02.6）
std::unique_ptr<Pass> CreatePrintOpNamesPass() {
  return std::make_unique<PrintOpNamesPass>();
}

// ------------------------------------------------------------
// ② 注册：把 Pass 挂成一个命令行选项
// ------------------------------------------------------------
// 这行代码在 main 之前执行（全局变量的构造函数），向全局注册表登记：
// 以后命令行写 --print-op-names，PassManager 就会往流水线里塞这个 Pass
// ⚠️ 踩坑实录（MLIR 15）：EmptyPipelineOptions 有模板特化，回调只收一个
//    参数，且类型必须是 OpPassManager&（不是 PassManager&！两者是子类/父类
//    关系，std::function 不认这种"反向"转换，报错信息还很有迷惑性）
static PassPipelineRegistration<> printOpNamesPipeline(
    "print-op-names",                    // 命令行选项名
    "打印每个 Operation 的名字",           // 帮助文档
    [](OpPassManager &pm) {
      pm.addPass(CreatePrintOpNamesPass());
    });

// ------------------------------------------------------------
// ③ 工具主体：站在官方 mlir-opt 的肩膀上
// ------------------------------------------------------------
// MlirOptMain 就是 mlir-opt 这个命令的 main。我们自己写的工具链上它之后：
//   - --print-op-names 是我们新加的
//   - --canonicalize、--cse 等官方 Pass 也全部能用
int main(int argc, char **argv) {
  DialectRegistry registry;
  registerAllDialects(registry);  // 让工具认识 func/arith/memref/... 所有方言
  // MlirOptMain 返回 LogicalResult（成功/失败），asMainReturnCode 转成进程退出码
  return asMainReturnCode(
      MlirOptMain(argc, argv, "我的第一个 mlir-opt 工具\n", registry));
}
