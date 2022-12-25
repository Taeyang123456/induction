#include "kinduction/k_induction.hpp"
#include "kinduction/spec.hpp"
#include "kinduction/loop_info.hpp"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Pass.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Transforms/Scalar.h>
#include "llvm/Analysis/CFG.h"
#include "llvm/Support/Casting.h"
#include "llvm/Transforms/IPO.h"
#include <llvm/Transforms/Utils.h>
#include <vector>
#include "llvm/IR/InstIterator.h"
#include <iostream>
#include <stack>
#include <queue>


using namespace std;
using namespace llvm;

static std::vector<CFGNode> CFGNodeVec;

KInduction::Result KInduction::verify(const Module& t_M, const unsigned k){

	outs()<<"+=========== Verifying ===========+\n";
	if(hasNotExpandedCalls(t_M, g_SpecFuncNames)){
		return UNKNOWN;
	}
	unique_ptr<llvm::Module> M = CloneModule(t_M);
	standardize(*M);
	
	Function* MainFunc = M->getFunction("main");
	vector<LabeledCondition> SpecConds = findSpecConditions(MainFunc);
	if(!hasAssertions(SpecConds)){
		return TRUE;  
	}
    
	map<Function*, vector<LCSSA>> LCSSAInfo;
	{
		legacy::PassManager PM;
		PM.add(new WrapLCSSAInfoPass(LCSSAInfo));
		PM.run(*M);
	}
	vector<LCSSA>& MainLCSSAs = LCSSAInfo[MainFunc];

    buildCFG(MainLCSSAs, MainFunc);

    printCFGNode();

    vector<int> initNode;
    collectInitBasicBlock(initNode);

    assert(initNode.size() > 0);

    // cout << "find path : " << endl;
    // for(int i = 0; i < initNode.size(); i++)
    //     cout << CFGNodeVec[initNode[i]].bb->getName().str() << endl;
    
    int assertLoopIdx = findVerifyLoop(MainLCSSAs);
    assert(assertLoopIdx != -1);
    cout << "find assert in : " << assertLoopIdx << endl;

    baseCase(initNode, assertLoopIdx, LCSSAInfo, k);    

	return UNKNOWN;
}

void KInduction::standardize(Module& M){
	legacy::PassManager PM;
	PM.add(createLoopRotatePass());
	PM.run(M);
}

bool KInduction::hasNotExpandedCalls(const Module& M,const set<string>& ignore){
	Function* MainFunc = M.getFunction("main");
	for(inst_iterator it = inst_begin(MainFunc), end = inst_end(MainFunc);it!=end;it++){
		if(CallInst* call = dyn_cast<CallInst>(&(*it))){
			Function* callee = call->getCalledFunction();
			if(!callee->isDeclaration() && !ignore.count(callee->getName().str())){
				outs() << "Not ExpandedCalls : " << callee->getName()<<"\n";
				return true;
			}
		}
	}
	return false;
}

bool KInduction::hasAssertions(const vector<LabeledCondition>& LCs){
	unsigned numAssert = 0u;
	for(auto LC : LCs){
		if(LabeledCondition::Label::ASSERT == LC.label){
			numAssert++;
		}
	}
	return numAssert;
}


void KInduction::buildCFG(vector<LCSSA>& LCSSAInfo, Function* mainFunc) {
    
    // create a CFGNode struct for every basic block
    // for(auto iter = mainFunc->begin(); iter != mainFunc->end(); iter++) {
    //     cout << iter->getName().str() << endl;
    //     CFGNodeVec.push_back(CFGNode(&*iter));
    // }
    // cout << "EEEEND\n";
    

    BasicBlock& entryBlock = mainFunc->getEntryBlock();
    queue<BasicBlock*> q;
    set<BasicBlock*> visit;
    q.push(&entryBlock);
    while(!q.empty()) {
        BasicBlock* bb = q.front();
        q.pop();
        if(visit.find(bb) != visit.end()) {
            continue;
        }
        visit.insert(bb);

        // determine whether this BasicBlock is in a loop
        bool isLoop = false;
        int idx = -1;
        for(unsigned i = 0; i < LCSSAInfo.size(); i++) {
            if(find(LCSSAInfo[i].bbVec.begin(), LCSSAInfo[i].bbVec.end(), bb) != LCSSAInfo[i].bbVec.end()) {
                isLoop = true;
                idx = i;
            }
        }

        int parent_idx = -1;
        vector<CFGNode>::iterator iter = find_if(CFGNodeVec.begin(), CFGNodeVec.end(),
            [&bb](const CFGNode& n) -> bool { return bb == n.bb; });

        if(iter == CFGNodeVec.end()) {
            CFGNodeVec.push_back(CFGNode(bb));
            iter = CFGNodeVec.begin() + CFGNodeVec.size() - 1;
        }
        parent_idx = iter - CFGNodeVec.begin();
        assert(parent_idx != -1);

        assert(iter != CFGNodeVec.end());
        iter->isLoopNode = isLoop;
        iter->LCSSAIndex = idx;

        // cout << iter->bb->getName().str() << " is loop : " << iter->isLoopNode << " has children : " << endl;

        for(int i = 0; i < bb->getTerminator()->getNumSuccessors(); i++) {
            BasicBlock* succBB = bb->getTerminator()->getSuccessor(i);
            vector<CFGNode>::iterator temp_iter = find_if(CFGNodeVec.begin(), CFGNodeVec.end(),
                [&succBB](const CFGNode& n) -> bool { return succBB == n.bb; });
            
            if(temp_iter == CFGNodeVec.end()) {
                CFGNodeVec.push_back(CFGNode(succBB));
                temp_iter = CFGNodeVec.begin() + CFGNodeVec.size() - 1;
            }
            // cout << "creating a child : " << temp_iter->bb->getName().str() << endl;
            CFGNodeVec[parent_idx].childs.push_back(temp_iter - CFGNodeVec.begin());
            // cout << "iter-childs size = " << iter->childs.size() << endl;
            q.push(succBB);

            // printCFGNode();
        }
        // CFGNodeVec.push_back(node);
    }
    
}

void KInduction::collectInitBasicBlock(vector<int>& initNode) {
    assert(CFGNodeVec.size() > 0);
    initNode.push_back(0);
    basicBlockDFS(initNode);
}

bool KInduction::basicBlockDFS(vector<int>& vec) {

    assert(vec.size() >= 1);
    // cout << "print vec : " << endl;
    // for(int i = 0; i < vec.size(); i++) {
    //     cout << vec[i] << endl;
    // }
    // cout << endl;
    
    int lastNodeIdx = vec.back();
    if(CFGNodeVec[lastNodeIdx].isLoopNode) {
        return true;
    }

    int path_count = 0;
    vector<int> tmp;
    
    for(int& nextNodeIdx : CFGNodeVec[lastNodeIdx].childs) {
        // cout << "vec push : " << nextNodeIdx << endl;
        vec.push_back(nextNodeIdx);
        if(basicBlockDFS(vec)) {
            tmp = vec;
            path_count++;
        }
        // cout << "vec pop back : " << vec.back() << endl;
        vec.pop_back();
    }
    
    assert(path_count <= 1);
    vec = tmp;
    if(path_count == 0)
        return false;
    else
        return true;
}

// if the loop's BB lead to a BB contains reach_error, than it's the verify loop
int KInduction::findVerifyLoop(vector<LCSSA>& LCSSAInfo) {
    int index = -1;
    assert(LCSSAInfo.size() > 0);
    for(int i = 0; i < LCSSAInfo.size(); i++) {
        vector<BasicBlock*> bbVec = LCSSAInfo[i].bbVec;
        assert(bbVec.size() > 0);
        for(int j = 0; j < bbVec.size(); j++) {
            bool found = false;
            BasicBlock* loopBB = bbVec[j];
            for(auto it = succ_begin(loopBB); it != succ_end(loopBB); it++) {
                BasicBlock* succBB = *it;
                for(Instruction& I : *succBB) {
                    if(isa<CallInst>(I)) {
                        CallInst& callInst = cast<CallInst>(I);
                        if(callInst.getCalledFunction()->getName().str() == "reach_error") {
                            assert(index == -1);
                            index = i;
                            found = true;
                            break;
                        }
                    }
                }
                if(found) break;
            }
            if(found) break;
        }
    }
    return index;
}

void printCFGNode() {
    cout << "CFG Node size = " << CFGNodeVec.size() << endl;
    for(int i = 0; i < CFGNodeVec.size(); i++) {
        cout << "node " << i << " : " << CFGNodeVec[i].bb->getName().str() << " is Loop = "
        << CFGNodeVec[i].isLoopNode << " LCSSA Index = " << CFGNodeVec[i].LCSSAIndex << endl;
        cout << "\t children (" << CFGNodeVec[i].childs.size() << "): ";
        for(int j = 0; j < CFGNodeVec[i].childs.size(); j++)
            cout << CFGNodeVec[i].childs[j] << "\t";
        cout << endl;
    }
}