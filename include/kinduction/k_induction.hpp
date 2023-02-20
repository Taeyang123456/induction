#ifndef KINDUCTION_KINDUCTION_HPP
#define KINDUCTION_KINDUCTION_HPP

#include <llvm/IR/Module.h>
#include "kinduction/cfg.hpp"
#include "kinduction/spec.hpp"
#include "kinduction/loop_info.hpp"



#include <map>
#include <z3++.h>

#define DEFAULT_ARG_K 1


struct expr_info {
    int index = -1;
    int times = 0;
    expr_info() {}
    expr_info(int a, int b) { index = a; times = b; }
};

class KInduction {
public:
	static inline unsigned getDefaultK(){
		return DEFAULT_ARG_K;
	}
	enum Result{UNKNOWN, FALSE, TRUE};
	static Result verify(const llvm::Module&, const unsigned = DEFAULT_ARG_K);	
private:

    static bool isFirstNondet;

    static void buildCFG(std::vector<LCSSA>&, llvm::Function*);
	static void standardize(llvm::Module&);
	static bool hasNotExpandedCalls(const llvm::Module&, const std::set<std::string>&);
	static bool hasAssertions(const std::vector<LabeledCondition>&);
    static void collectInitBasicBlock(std::vector<std::vector<int>>&);
    static void collectVerifyPathErrInLoop(std::vector<Path>&, std::vector<int>&, std::vector<LCSSA>&, int);
    static void collectVerifyPathErrInNode(std::vector<Path>&, std::vector<int>&, std::vector<LCSSA>&, int);
    static bool basicBlockDFS(std::vector<std::vector<int>>&, std::vector<int>&);
    static int findVerifyLoop(std::vector<LCSSA>&);
    static int findVerifyNode();
    static bool baseCase(std::vector<Path>&, std::vector<LCSSA>&, unsigned);
    static bool baseCaseSMTChecking(std::vector<int>& , int);
    static bool inductiveStep(std::vector<Path>&, std::vector<LCSSA>&, unsigned);
    static bool inductiveStepSMTChecking(std::vector<int>&, int);
    static int getCFGNodeVecIndexByBB(const llvm::BasicBlock*);

    static z3::expr handleBinaryOp(llvm::Instruction&, z3::expr_vector&, std::map<llvm::Value*, expr_info>&, std::map<llvm::Value*, int>&, z3::context&);
    static z3::expr handleCmpOp(llvm::Instruction&, z3::expr_vector&, std::map<llvm::Value*, expr_info>&, std::map<llvm::Value*, int>&, z3::context&);

    static std::string valueGetName(llvm::Value*, std::map<llvm::Value*, int>&);
    static z3::expr getExpr(llvm::Value*, z3::expr_vector&, std::map<llvm::Value*, expr_info>&, std::map<llvm::Value*, int>&, z3::context&);
    static z3::expr getExprWithRefresh(llvm::Value*, z3::expr_vector&, std::map<llvm::Value*, expr_info>&, std::map<llvm::Value*, int>&, z3::context&);

    static void clear() { isFirstNondet = true; }
    static void printProblemAndSolution(z3::solver&, z3::expr_vector&, z3::expr_vector&);

};

void printCFGNode();

#endif
