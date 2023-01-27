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
    CFGNodeVec.clear();
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

    int assertLoopIdx = findVerifyLoop(MainLCSSAs);
    assert(assertLoopIdx != -1);
    cout << "find assert in : " << assertLoopIdx << endl;

    vector<int> initNode;
    collectInitBasicBlock(initNode);
    assert(initNode.size() > 0);
    
    vector<Path> path;
    collectVerifyPath(path, initNode, MainLCSSAs, assertLoopIdx);

    cout << "PRINT PATH:\n";
    for(int i = 0; i < path.size(); i++) {
        if(path[i].type == Path::NodeType::ANode) {
            cout << "Node : " << path[i].nodeIdx << endl;
        }
        else {
            cout << "Loop : " << path[i].loopLCSSAIdx << endl;
        }
    }



    

    // cout << "find path : " << endl;
    // for(int i = 0; i < initNode.size(); i++)
    //     cout << CFGNodeVec[initNode[i]].bb->getName().str() << endl;
    
    



    baseCase(path, assertLoopIdx, MainLCSSAs, k);    

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

void KInduction::collectVerifyPath(vector<Path>& path, vector<int>& initNodes, vector<LCSSA>& LCSSAs, 
                                int assertLoopIdx) {

    assert(CFGNodeVec[initNodes.back()].isLoopNode);
    assert(initNodes.size() > 0);
    vector<bool> visit(CFGNodeVec.size(), false);
    for(int i = 0; i < initNodes.size() - 1; i++) {
        path.push_back(Path(Path::NodeType::ANode, initNodes[i]));
        visit[initNodes[i]] = true;
    }

    path.push_back(Path(Path::NodeType::ALoop, CFGNodeVec[initNodes.back()].LCSSAIndex));

    while(true) {
        if(path.back().type == Path::NodeType::ALoop && path.back().loopLCSSAIdx == assertLoopIdx)
            break;
        if(path.back().type == Path::NodeType::ANode) {
            int nodeIdx = path.back().nodeIdx;
            bool findNextStep = false;
            for(int i = 0; i < CFGNodeVec[nodeIdx].childs.size(); i++) {
                if(!visit[CFGNodeVec[nodeIdx].childs[i]]) {
                    visit[CFGNodeVec[nodeIdx].childs[i]] = true;
                    findNextStep = true;
                    if(CFGNodeVec[CFGNodeVec[nodeIdx].childs[i]].isLoopNode) {
                        path.push_back(Path(Path::NodeType::ALoop, CFGNodeVec[CFGNodeVec[nodeIdx].childs[i]].LCSSAIndex));
                    }
                    else {
                        path.push_back(Path(Path::NodeType::ANode, CFGNodeVec[nodeIdx].childs[i]));
                    }
                    break;
                }
            }
            if(!findNextStep) {
                assert(path.size() > 0);
                path.pop_back();
            }
        }
        else {
            int lcssaIdx = path.back().loopLCSSAIdx;
            for(int i = 0; i < LCSSAs[lcssaIdx].bbVec.size(); i++) {
                visit[getCFGNodeVecIndexByBB(LCSSAs[lcssaIdx].bbVec[i])] = true;
            }

            const BasicBlock* lastBB = LCSSAs[lcssaIdx].backedge.first;
            int lastBBIdx = getCFGNodeVecIndexByBB(lastBB);

            bool findNextStep = false;
            for(int i = 0; i < CFGNodeVec[lastBBIdx].childs.size(); i++) {
                if(!visit[CFGNodeVec[lastBBIdx].childs[i]]) {
                    visit[CFGNodeVec[lastBBIdx].childs[i]] = true;
                    findNextStep = true;
                    if(CFGNodeVec[CFGNodeVec[lastBBIdx].childs[i]].isLoopNode) {
                        path.push_back(Path(Path::NodeType::ALoop, CFGNodeVec[CFGNodeVec[lastBBIdx].childs[i]].LCSSAIndex));
                    }
                    else {
                        path.push_back(Path(Path::NodeType::ANode, CFGNodeVec[lastBBIdx].childs[i]));
                    }
                }
            }
            if(!findNextStep) {
                assert(path.size() > 0);
                path.pop_back();
            }
        }
    }
    return ;
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


bool KInduction::baseCase(vector<Path>& path, int assertLoopIdx, vector<LCSSA>& LCSSAs, unsigned k) {
    
    // for(int i = 0; i < initNodes.size(); i++) {
    //     cout << "initNode : " << initNodes[i] << " \t";
    // }
    // cout << endl;

    // cout << "assertLoopIdx = " << assertLoopIdx << endl;

    // cout << "LCSSAs : " << endl;
    // for(int i = 0; i < LCSSAs.size(); i++) {
    //     cout << LCSSAs[i].bbVec.size() << endl << "\t";
    //     for(int j = 0; j < LCSSAs[i].bbVec.size(); j++) {
    //         errs() << LCSSAs[i].bbVec[j]->getName() << "\n";
    //     }
    // }
    // cout << endl;

    vector<int> baseNodeIdx;

    for(int i = 0; i < path.size(); i++) {
        if(path[i].type == Path::NodeType::ANode) {
            baseNodeIdx.push_back(path[i].nodeIdx);
        }
        else if(path[i].type == Path::ALoop) {
            int lcssaIdx = path[i].loopLCSSAIdx;
            errs() << "backedge first = " << LCSSAs[lcssaIdx].backedge.first->getName() << "\n"
                    << "backedge second = " << LCSSAs[lcssaIdx].backedge.second->getName() << "\n";
            assert(LCSSAs[lcssaIdx].backedge.first == LCSSAs[lcssaIdx].bbVec[0] 
                    && LCSSAs[lcssaIdx].backedge.second == LCSSAs[lcssaIdx].bbVec.back());
            for(int j = 0; j < k; j++) {
                for(int loop_bb_count = 0; loop_bb_count < LCSSAs[lcssaIdx].bbVec.size(); loop_bb_count++) {
                    baseNodeIdx.push_back(getCFGNodeVecIndexByBB(LCSSAs[lcssaIdx].bbVec[loop_bb_count]));
                }
            }
        }
        else {
            assert(false);
        }
    }

    cout << "PRINT BASE CASE PATH : " << endl;
    for(int i = 0; i < baseNodeIdx.size(); i++) {
        cout << baseNodeIdx[i] << "\t";
    }
    cout << endl;


    return true;
}

int KInduction::getCFGNodeVecIndexByBB(const llvm::BasicBlock* bb) {
    for(int i = 0; i < CFGNodeVec.size(); i++) {
        if(CFGNodeVec[i].bb == bb)
            return i;
    }
    assert(false);
    return -1;
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