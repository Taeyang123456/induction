#ifndef KINDUCTION_SPEC_HPP
#define KINDUCTION_SPEC_HPP

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include <vector>
#include <set>

extern std::set<std::string> g_SpecFuncNames; 

struct LabeledCondition{
	enum Label{ASSUME,ASSERT};
	const llvm::Value* condition;
	bool isTrueBr;
	Label label;
};


std::vector<LabeledCondition> findSpecConditions(const llvm::Function*);


#endif
