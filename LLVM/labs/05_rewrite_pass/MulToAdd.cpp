// ============================================================
// MulToAdd.cpp —— 第二个 Pass：用 RewritePattern "改写" IR
// 【前置标注】需要：labs/01_my_first_pass（Pass 骨架）、cpp C++ 继承/虚函数
//
// 人话版：01 里的 Pass 只会"看"（打印名字）。真正的优化 Pass 要会"改"。
//   MLIR 改 IR 的主流写法 = 定一堆 RewritePattern（改写规则），
//   交给 PatternRewriteDriver 引擎，引擎自动帮你匹配+替换+保持 IR 合法。
//
// 本 Pass 做的事（教学经典）：
//   arith.addf %x, %x   ==>   arith.mulf %x, 2.0
//   （x+x 换成 x*2，浮点版；整数版 x+x→x<<2 是移位，这里不碰）
//
// 为什么这不算"乱优化"：浮点里 x+x 和 x*2 结果完全一致（都是精确翻倍，
// 不涉及舍入变化），所以是安全变换。对比：x+0.0 就不能换成 x（-0.0 坑），
// 见手册 L03 的 canonicalize 实验。
// ============================================================

#include "mlir/Dialect/Arithmetic/IR/Arithmetic.h"  // arith::AddFOp 等
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/DialectRegistry.h"
#include "mlir/InitAllDialects.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Pass/PassManager.h"
#include "mlir/Pass/PassRegistry.h"
#include "mlir/Tools/mlir-opt/MlirOptMain.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"  // applyPatternsAndFoldGreedily
#include "llvm/ADT/APFloat.h"
#include "llvm/Support/raw_ostream.h"

using namespace mlir;

// ------------------------------------------------------------
// 改写规则：一条规则 = 一个类
// ------------------------------------------------------------
// 读法：OpRewritePattern<arith::AddFOp> 意思是
//   "我这条规则只对 arith.addf 这种 Operation 感兴趣"
// 引擎会先把 IR 里所有 addf 挑出来，逐个调 matchAndRewrite 问你改不改
struct AddFSelfToMulF : public OpRewritePattern<arith::AddFOp> {
  // 从父类继承构造函数：拿到 context（创建新 op 需要它）
  using OpRewritePattern<arith::AddFOp>::OpRewritePattern;

  // 返回 success() = 我改了；返回 failure() = 这条不匹配，别动它
  LogicalResult matchAndRewrite(arith::AddFOp op,
                                PatternRewriter &rewriter) const override {
    Value lhs = op.getLhs();
    Value rhs = op.getRhs();

    // 只处理 x + x（左右是完全同一个 Value 的情况）
    if (lhs != rhs)
      return failure();

    // rewriter 负责"安全地"改 IR（它会在幕后处理引用计数、DAG 更新）
    // 对照：直接 op->erase() 之类手改，极易把 IR 改坏，MLIR 不推荐
    auto two = rewriter.create<arith::ConstantFloatOp>(
        op.getLoc(), APFloat(2.0f), rewriter.getF32Type());
    rewriter.replaceOpWithNewOp<arith::MulFOp>(op, lhs, two);
    return success();
  }
};

// ------------------------------------------------------------
// Pass 本体：把规则收集起来，交给贪婪引擎跑
// ------------------------------------------------------------
struct MulToAddPass
    : public PassWrapper<MulToAddPass, OperationPass<func::FuncOp>> {
  // 注意这次是 func::FuncOp 级别（函数级优化，不用等 ModuleOp）
  void runOnOperation() override {
    RewritePatternSet patterns(&getContext());
    patterns.add<AddFSelfToMulF>(&getContext());  // 登记我们的规则
    // 贪婪驱动：反复扫 IR，直到没有规则能再用为止
    (void)applyPatternsAndFoldGreedily(getOperation(), std::move(patterns));
  }
};

std::unique_ptr<Pass> CreateMulToAddPass() {
  return std::make_unique<MulToAddPass>();
}

static PassPipelineRegistration<> mulToAddPipeline(
    "mul-to-add",
    "把 addf %x, %x 改写成 mulf %x, 2.0",
    [](OpPassManager &pm) { pm.addPass(CreateMulToAddPass()); });

int main(int argc, char **argv) {
  DialectRegistry registry;
  registerAllDialects(registry);
  return asMainReturnCode(
      MlirOptMain(argc, argv, "我的第二个 mlir-opt 工具（改写版）\n", registry));
}
