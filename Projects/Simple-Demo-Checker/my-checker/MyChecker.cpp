#include <iostream>

#include "ClangSACheckers.h"
#include "clang/StaticAnalyzer/Core/BugReporter/BugType.h"
#include "clang/StaticAnalyzer/Core/Checker.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CallEvent.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CheckerContext.h"

using namespace clang;
using namespace clang::ento;

namespace {
class MyChecker:public Checker<check::PreCall> {
    mutable std::unique_ptr<BugType> BT;
public:
    void checkPreCall (const CallEvent &Call, CheckerContext &C) const;
};
}//end of anonymous namespace

bool lt_irq_enable = true;

void MyChecker::checkPreCall(const CallEvent &Call, CheckerContext &C) const {
    if (const IdentifierInfo *II = Call.getCalleeIdentifier()) {
        if (II->isStr("main")) {
            if (!BT)
                BT.reset(new BugType(this, "Call to main", "Example checker"));
            ExplodedNode *N = C.generateErrorNode();
            auto Report = llvm::make_unique <BugReport>(*BT, BT->getName(), N);
            C.emitReport(std::move(Report));
        }
        if (II->isStr("arch_local_irq_save"))
            lt_irq_enable = false;
        if (II->isStr("arch_local_irq_restore"))
            lt_irq_enable = true;
        if (II->isStr("__might_sleep")) {
            if (lt_irq_enable == false) {
                if (!BT)
                    BT.reset(new BugType(this, "sleep while local_irq_save", "Example checker"));
                ExplodedNode *N = C.generateErrorNode();
                auto Report = llvm::make_unique <BugReport>(*BT, BT->getName(), N);
                C.emitReport(std::move(Report));
            }
        }
    }
}

void ento::registerMyChecker(CheckerManager &Mgr) {
    Mgr.registerChecker<MyChecker>();
}