#include "kinduction/spec.hpp"
#include "kinduction/utils.hpp"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/Casting.h"
#include "llvm/Analysis/CFG.h"
#include <cassert>
#include <cstddef>
#include <vector>
#include <list>
#include <set>
using namespace std;
using namespace llvm;

set<string> g_SpecFuncNames = {"abort", "reach_error"};

vector<LabeledCondition> findSpecConditions(const Function* F){
	
	set<pair<const BasicBlock*, const BasicBlock*>> backedges;
	{
		SmallVector<pair<const BasicBlock*,const BasicBlock*>,5> BEs;
		FindFunctionBackedges(*F,BEs);
		backedges.insert(BEs.begin(),BEs.end()); 
	}

	vector<LabeledCondition> LCs;
	const BasicBlock* EntryBB = &F->getEntryBlock();
	list<const BasicBlock*> worklist{EntryBB};
	set<const BasicBlock*> visited;
	while (!worklist.empty()) {
		auto BB = worklist.front();
		worklist.pop_front();
		visited.insert(BB);
		for(auto &I : *BB){
			if(auto call = dyn_cast<CallInst>(&I)){
				auto callee = call->getCalledFunction()->getName();
				LabeledCondition::Label label;
				if("abort" == callee){
					label = LabeledCondition::Label::ASSUME;
				}
				else if("reach_error" == callee){
					label = LabeledCondition::Label::ASSERT;
				}
				else{
					continue;
				}
				set<pair<const BasicBlock*,bool>> BrPts;
				findBranchPoints(BB,BrPts);
				for(auto BrBB : BrPts){
					if(BrBB.first){
						auto br = dyn_cast<BranchInst>(BrBB.first->getTerminator());
						LabeledCondition LC;
						LC.condition = br->getCondition();
						LC.isTrueBr = BrBB.second;
						LC.label = label;
						LCs.emplace_back(LC);
					}
					else{
						assert(0 && "The program must reach 'abort()' or 'reach_error()'.");
					}
				}
			}
			else if(auto ret = dyn_cast<ReturnInst>(&I)){
				if(auto CI = dyn_cast<ConstantInt>(ret->getReturnValue())){
					auto RetValue =  CI->getValue().getSExtValue();
					if(RetValue){
						set<pair<const BasicBlock*, bool>> BrPts;
						findBranchPoints(BB, BrPts);
						for(auto BrBB : BrPts){
							if(BrBB.first){
								auto br = dyn_cast<BranchInst>(BrBB.first->getTerminator());
								LabeledCondition LC;
								LC.condition = br->getCondition();
								LC.isTrueBr = BrBB.second;
								LC.label = LabeledCondition::Label::ASSERT;
								LCs.emplace_back(LC);
							}
							else{
								assert(0 && "The program must reach 'abort()' or 'reach_error()'.");
							}
						}
					}
				}
				else if(auto RetInst =dyn_cast<Instruction>(&I)){
					LabeledCondition LC;
					LC.condition = RetInst;
					LC.label = LabeledCondition::ASSERT;
					LC.isTrueBr = true;
					LCs.emplace_back(LC);	
				}
			}
		}
		auto term = BB->getTerminator();
		for(auto succ = succ_begin(term);succ!= succ_end(term);succ++){
			if(backedges.count({BB,*succ})){
				continue;
			}
			bool visitedPreds = true;
			for(auto pred=pred_begin(*succ);pred!=pred_end(*succ);pred++){
				if(backedges.count({*pred,*succ})){
					continue;
				}
				if(!visited.count(*pred)){
					visitedPreds = false;
					break;
				}
			}
			if(visitedPreds){
				worklist.emplace_back(*succ);
			}
		}

	}
	return LCs;
}
