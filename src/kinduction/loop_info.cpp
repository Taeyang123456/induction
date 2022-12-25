#include "kinduction/loop_info.hpp"
#include "llvm/ADT/StringRef.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Pass.h"
#include <memory>
#include <vector>
using namespace std;
using namespace llvm;

char WrapLCSSAInfoPass::ID(0);
// static llvm::RegisterPass<WrapLCSSAInfoPass> X("WrapLCSSAInfo","Wrap LCSSA Info Pass",true,true);

WrapLCSSAInfoPass::WrapLCSSAInfoPass(map<Function*,vector<LCSSA>> &info)
	:LoopPass(ID),m_LCSSAInfo(info){}

bool WrapLCSSAInfoPass::runOnLoop(Loop* L, LPPassManager &LPPM){
	ScalarEvolution &SE = getAnalysis<ScalarEvolutionWrapperPass>().getSE();
	LCSSA KeyElem;
	BasicBlock* header = L->getHeader();
	BasicBlock* latch = L->getLoopLatch();
	KeyElem.backedge = {header, latch};
	KeyElem.IndVar = L->getInductionVariable(SE);
    // outs() << header->getName() << " " << latch->getName() << "\n";

    // record loop's correspond bb
    KeyElem.bbVec = L->getBlocksVector();
    
    // outs() << "bbVec size = " << KeyElem.bbVec.size() << "\n";
    // for(int i = 0; i < KeyElem.bbVec.size(); i++) {
    //     outs() << KeyElem.bbVec[i]->getName().str() << "\n";
    // }

    Optional<Loop::LoopBounds> bounds = L->getBounds(SE);
    
	if(bounds.hasValue()) {
        // errs() << "has value : " <<  bounds->getInitialIVValue() << " " << bounds->getStepValue() << " "
            // << bounds->getFinalIVValue() << "\n";
		KeyElem.IVDesc = make_shared<IVDescriptor>(&(bounds->getInitialIVValue()),
				&(bounds->getStepInst()),bounds->getStepValue(),
				&(bounds->getFinalIVValue()),bounds->getDirection());
	}
	KeyElem.depth = L->getLoopDepth();
	Function* F = header->getParent();
	m_LCSSAInfo[F].emplace_back(KeyElem);

	return false;
}
void WrapLCSSAInfoPass::getAnalysisUsage(AnalysisUsage &AU) const{
	AU.setPreservesAll();
	AU.addRequired<ScalarEvolutionWrapperPass>();
}

StringRef WrapLCSSAInfoPass::getPassName() const {
	return "WrapLCSSAInfoPass";
}
