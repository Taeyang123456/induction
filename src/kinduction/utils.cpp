#include "kinduction/utils.hpp"
#include "kinduction/defs.hpp"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Instructions.h"
using namespace llvm;
using namespace std;

void findBranchPoints(const BasicBlock* BB,set<pair<const BasicBlock*,bool>> &BrPts){
	auto it = pred_begin(BB);
	auto end = pred_end(BB);
	if(it == end){
		BrPts.insert({nullptr,true});
	}
	else{
		for(; it != end; it++){
			auto PredBr = dyn_cast<BranchInst>((*it)->getTerminator());
			assert(PredBr);
			if(PredBr->isConditional()){
				if(BB == PredBr->getSuccessor(0)){
					BrPts.insert({*it,true});
				}
				else{
					BrPts.insert({*it,false});
				}
			}
			else{
				findBranchPoints(*it, BrPts);
			}
		}
	}
}
