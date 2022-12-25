#ifndef KINDUCTION_LOOPINFO_HPP
#define KINDUCTION_LOOPINFO_HPP
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include <llvm/IR/Instructions.h>
#include <llvm/Analysis/LoopPass.h>
#include <llvm/Analysis/IVDescriptors.h>
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Value.h"
#include <memory>
#include <vector>
#include <map>

#define Edge std::pair<const llvm::BasicBlock*, const llvm::BasicBlock*>

/*
 * Induction Variable Descriptor
 */
struct IVDescriptor{
	llvm::Value* InitValue;
	llvm::Value* StepInst;
	llvm::Value* StepValue;
	llvm::Value* FinalValue;
	llvm::Loop::LoopBounds::Direction direction;
	IVDescriptor(llvm::Value* IV,llvm::Value* SI, llvm::Value* SV,llvm::Value* FV, llvm::Loop::LoopBounds::Direction D)
		:InitValue(IV),StepInst(SI),StepValue(SV),FinalValue(FV),direction(D){}
};

/*
 * LCSSA stands for Loop-closed SSA form
 */
struct LCSSA{
	Edge backedge = {nullptr, nullptr};
	llvm::PHINode *IndVar = nullptr;
	std::shared_ptr<IVDescriptor> IVDesc;
	unsigned depth = 0u;
    std::vector<llvm::BasicBlock*> bbVec;
};

class WrapLCSSAInfoPass : public llvm::LoopPass{

public:
	WrapLCSSAInfoPass(std::map<llvm::Function*, std::vector<LCSSA>>&);
	bool runOnLoop(llvm::Loop*, llvm::LPPassManager&) override;
	void getAnalysisUsage(llvm::AnalysisUsage&) const override;
	llvm::StringRef getPassName() const override;
public:
	static char ID;
private:
	std::map<llvm::Function*, std::vector<LCSSA>> &m_LCSSAInfo;	
};

#endif
