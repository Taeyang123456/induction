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
#include <z3++.h>


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
        else if(path[i].type == Path::NodeType::ALoop) {
            cout << "Loop : " << path[i].loopLCSSAIdx << endl;
        }
        else if(path[i].type == Path::NodeType::ERRORLOOP) {
            cout << "ERRORLOOP : " << endl;
        }
        else if(path[i].type == Path::NodeType::ERRORNODE) {
            cout << "ERRORNODE : " << endl;
        }
        else {
            assert(false);
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
        if(path.back().type == Path::NodeType::ALoop && path.back().loopLCSSAIdx == assertLoopIdx) {
            path.back().type = Path::NodeType::ERRORLOOP;
            break;
        }
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


    // add ERROR block to path
    int errorLoop = path.back().loopLCSSAIdx;
    int errorLoopLastBB = getCFGNodeVecIndexByBB(LCSSAs[errorLoop].backedge.first);
    int i = 0;
    for(; i < CFGNodeVec[errorLoopLastBB].childs.size(); i++) {
        int childIdx = CFGNodeVec[errorLoopLastBB].childs[i];
        errs() << CFGNodeVec[childIdx].bb->getName() << "\n";
        if(CFGNodeVec[childIdx].bb->getName().str().find("ERROR") != string::npos) {
            path.push_back(Path(Path::NodeType::ERRORNODE, childIdx));
            break;
        }
    }
    assert(i != CFGNodeVec[errorLoopLastBB].childs.size());
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

    // Pattern 1 : VERIFIER_assert in a loop
    if(path.size() >= 2 && path.back().type == Path::NodeType::ERRORNODE &&
        path[path.size() - 2].type == Path::NodeType::ERRORLOOP) {
        vector<int> baseNodeIdx;

        for(int errorLoopCount = 1; errorLoopCount <= k; errorLoopCount++) {
            for(int i = 0; i < path.size(); i++) {
                if(path[i].type == Path::ANode) {
                    baseNodeIdx.push_back(path[i].nodeIdx);
                }
                else if(path[i].type == Path::ALoop) {
                    int lcssaIdx = path[i].loopLCSSAIdx;
                    assert(LCSSAs[lcssaIdx].backedge.first == LCSSAs[lcssaIdx].bbVec[0] 
                            && LCSSAs[lcssaIdx].backedge.second == LCSSAs[lcssaIdx].bbVec.back());
                    for(int j = 0; j < k; j++) {
                        for(int loop_bb_count = 0; loop_bb_count < LCSSAs[lcssaIdx].bbVec.size(); loop_bb_count++) {
                            baseNodeIdx.push_back(getCFGNodeVecIndexByBB(LCSSAs[lcssaIdx].bbVec[loop_bb_count]));
                        }
                    }
                }
                else if(path[i].type == Path::ERRORLOOP) {
                    int lcssaIdx = path[i].loopLCSSAIdx;
                    for(int j = 0; j < errorLoopCount; j++) {
                        for(int bbCount = 0; bbCount < LCSSAs[lcssaIdx].bbVec.size(); bbCount++) {
                            baseNodeIdx.push_back(getCFGNodeVecIndexByBB(LCSSAs[lcssaIdx].bbVec[bbCount]));
                        }
                    }
                }
                else if(path[i].type == Path::ERRORNODE) {
                    baseNodeIdx.push_back(path[i].nodeIdx);
                }
                else {
                    assert(false);
                }
            }

            // cout << "base node : " << endl;
            // for(int i = 0; i < baseNodeIdx.size(); i++) {
            //     cout << baseNodeIdx[i] << endl;
            // }
            baseCaseSMTChecking(baseNodeIdx, errorLoopCount);

            baseNodeIdx.clear();
        }
    }
    else {
        assert(false && "Base case Path type not support");
    }

    // cout << "PRINT BASE CASE PATH : " << endl;
    // for(int i = 0; i < baseNodeIdx.size(); i++) {
    //     cout << baseNodeIdx[i] << "\t";
    // }
    // cout << endl;


    // for(int i = 0; i < baseNodeIdx.size(); i++) {
    //     BasicBlock* bb = CFGNodeVec[i].bb;
    //     for(Instruction& inst : *bb) {
    //         errs() << inst << "\n";
    //     }
    // }


    return true;
}

bool KInduction::baseCaseSMTChecking(vector<int>& baseNodeIdx, int kval) {

    z3::context context;
    std::map<llvm::Value*, expr_info> val2exprIdx;
    z3::expr_vector exprs(context);


    z3::expr_vector problems(context);

    for(int i = 0; i < baseNodeIdx.size(); i++) {
        int nodeIdx = baseNodeIdx[i];
        BasicBlock* bb = CFGNodeVec[nodeIdx].bb;
        for(Instruction& I : *bb) {
            if(isa<CallInst>(I)) {
                CallInst& callInst = cast<CallInst>(I);
                if(callInst.getCalledFunction()->getName().str() == "__VERIFIER_nondet_int") {
                    Value* destVal = &callInst;
                    problems.push_back(getExprWithRefresh(destVal, exprs, val2exprIdx, context) == kval);
                }
                else {
                    assert(false);
                }
            }
            else if(isa<StoreInst>(I)) {
                StoreInst& storeInst = cast<StoreInst>(I);
                Value* srcVal = storeInst.getOperand(0);
                Value* destVal = storeInst.getOperand(1);
                assert(destVal->getType()->isPointerTy());
                
                if(GlobalValue* gv = dyn_cast<GlobalValue>(destVal)) {
                    problems.push_back(getExpr(srcVal, exprs, val2exprIdx, context) 
                            == getExprWithRefresh(destVal, exprs, val2exprIdx, context));
                }
                else {
                    assert(false);
                }
                
            }
            else if(isa<BinaryOperator>(I)) {
                problems.push_back(handleBinaryOp(I, exprs, val2exprIdx, context));
            }
            else if(isa<CmpInst>(I)) {
                problems.push_back(handleCmpOp(I, exprs, val2exprIdx, context));
            }
            else if(isa<BranchInst>(I)) {
                BranchInst& branchInst = cast<BranchInst>(I);
                if(branchInst.isConditional()) {
                    Value* conditionVal = branchInst.getOperand(0);
                    Value* bb1Val = branchInst.getOperand(2);
                    Value* bb2Val = branchInst.getOperand(1);

                    assert(i + 1 <= baseNodeIdx.size());
                    int nextNodeIdx = baseNodeIdx[i + 1];
                    BasicBlock* nextbb = CFGNodeVec[nextNodeIdx].bb;
                    if(nextbb->getName() == bb1Val->getName()) {
                        problems.push_back(getExpr(conditionVal, exprs, val2exprIdx, context) == 1);
                    }
                    else if(nextbb->getName() == bb2Val->getName()) {
                        problems.push_back(getExpr(conditionVal, exprs, val2exprIdx, context) == 0);
                    }
                    else {
                        assert(false);
                    }

                }
            }
            else {
                errs() << I << "\n";
                assert(false);
            }
        }
    }
}

int KInduction::getCFGNodeVecIndexByBB(const llvm::BasicBlock* bb) {
    for(int i = 0; i < CFGNodeVec.size(); i++) {
        if(CFGNodeVec[i].bb == bb)
            return i;
    }
    assert(false);
    return -1;
}

z3::expr KInduction::handleBinaryOp(Instruction& I, z3::expr_vector& exprs, map<Value*, expr_info>& val2exprIdx, z3::context& context) {
    z3::expr exprl(context);
    z3::expr exprr(context);

    Value* v1 = I.getOperand(0);
    Value* v2 = I.getOperand(1);

    if(ConstantInt* constantInt = dyn_cast<ConstantInt>(v1)) {
        exprl = context.bv_val(constantInt->getSExtValue(), constantInt->getType()->getPrimitiveSizeInBits());
    }
    else {
        exprl = getExpr(v1, exprs, val2exprIdx, context);
    }

    if(ConstantInt* constantInt = dyn_cast<ConstantInt>(v2)) {
        exprr = context.bv_val(constantInt->getSExtValue(), constantInt->getType()->getPrimitiveSizeInBits());
    }
    else {
        exprr = getExpr(v2, exprs, val2exprIdx, context);
    }

    

    if(isa<AddOperator>(I)) {
        return getExprWithRefresh(&I, exprs, val2exprIdx, context) == exprl + exprr;
    }
    else if(isa<ShlOperator>(I)) {
        return getExprWithRefresh(&I, exprs, val2exprIdx, context) == z3::shl(exprl, exprr);
    }
    else {
        outs() << I << "\n";
        assert(false);
    }
}

z3::expr KInduction::handleCmpOp(Instruction& I, z3::expr_vector& exprs, map<Value*, expr_info>& val2exprIdx, z3::context& context) {
    z3::expr exprl(context);
    z3::expr exprr(context);

    Value* v1 = I.getOperand(0);
    Value* v2 = I.getOperand(1);

    if(ConstantInt* constantInt = dyn_cast<ConstantInt>(v1)) {
        exprl = context.bv_val(constantInt->getSExtValue(), constantInt->getType()->getPrimitiveSizeInBits());
    }
    else {
        exprl = getExpr(v1, exprs, val2exprIdx, context);
    }

    if(ConstantInt* constantInt = dyn_cast<ConstantInt>(v2)) {
        exprr = context.bv_val(constantInt->getSExtValue(), constantInt->getType()->getPrimitiveSizeInBits());
    }
    else {
        exprr = getExpr(v2, exprs, val2exprIdx, context);
    }

    CmpInst& cmpInst = cast<CmpInst>(I);
    CmpInst::Predicate predicate = cmpInst.getPredicate();

    z3::expr condition(context);

    switch(predicate) {
        case CmpInst::Predicate::ICMP_ULT: condition = exprl < exprr; break;
        default: assert(false);
    }

    return getExprWithRefresh(&I, exprs, val2exprIdx, context)
        == z3::ite(condition, context.bv_val(1, 1), context.bv_val(0, 1));

}

z3::expr KInduction::getExpr(Value* v, z3::expr_vector& exprs, map<Value*, expr_info>& val2exprIdx, z3::context& context) {
    if(val2exprIdx.find(v) != val2exprIdx.end())
        return exprs[val2exprIdx[v].index];
    if(v->getType()->isIntegerTy()) {
        exprs.push_back(context.bv_const((v->getName().str() + "_0").c_str(), v->getType()->getPrimitiveSizeInBits()));
        val2exprIdx[v] = expr_info(exprs.size() - 1, 0);
        return exprs[val2exprIdx[v].index];
    }
    else {
        assert(false);
    }
}

z3::expr KInduction::getExprWithRefresh(Value* v, z3::expr_vector& exprs, map<Value*, expr_info>& val2exprIdx, z3::context& context) {
    if(val2exprIdx.find(v) != val2exprIdx.end()) {
        int idx = val2exprIdx[v].index;
        val2exprIdx[v].times++;
        if(GlobalValue* gv = dyn_cast<GlobalValue>(v)) {
            if(gv->getType()->isIntegerTy()) {
                exprs[idx] = context.bv_const((v->getName().str() + "_" + to_string(val2exprIdx[v].times)).c_str(),
                            v->getType()->getPrimitiveSizeInBits());
            }
            else {
                assert(false);
            }
        }
        else if(v->getType()->isIntegerTy()) {
            exprs[idx] = context.bv_const((v->getName().str() + "_" + to_string(val2exprIdx[v].times)).c_str(),
                            v->getType()->getPrimitiveSizeInBits());
        }
        else {
            assert(false);
        }
        return exprs[idx];
    }
    else {
        if(GlobalValue* gv = dyn_cast<GlobalValue>(v)) {
            assert(gv->getType()->isPointerTy());
            PointerType* pointerType = dyn_cast<PointerType>(gv->getType());
            if(pointerType->getElementType()->isIntegerTy()) {
                exprs.push_back(context.bv_const((v->getName().str() + "_0").c_str(), pointerType->getElementType()->getPrimitiveSizeInBits()));
                val2exprIdx[v] = expr_info(exprs.size() - 1, 0);
                return exprs[val2exprIdx[v].index];
            }
            else {
                assert(false);
            }
        }
        else if(v->getType()->isIntegerTy()) {
            exprs.push_back(context.bv_const((v->getName().str() + "_0").c_str(), v->getType()->getPrimitiveSizeInBits()));
            val2exprIdx[v] = expr_info(exprs.size() - 1, 0);
            return exprs[val2exprIdx[v].index];
        }
        else {
            errs() << *v->getType() << "\n";
            assert(false);
        }
    }
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