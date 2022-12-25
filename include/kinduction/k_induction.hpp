#ifndef KINDUCTION_KINDUCTION_HPP
#define KINDUCTION_KINDUCTION_HPP

#include <llvm/IR/Module.h>
#include "kinduction/cfg.hpp"
#include "kinduction/spec.hpp"
#include "kinduction/loop_info.hpp"

#define DEFAULT_ARG_K 1

class KInduction {
public:
	static inline unsigned getDefaultK(){
		return DEFAULT_ARG_K;
	}
	enum Result{UNKNOWN, FALSE, TRUE};
	static Result verify(const llvm::Module&, const unsigned = DEFAULT_ARG_K);	
private:

    

    static void buildCFG(std::vector<LCSSA>&, llvm::Function*);
	static void standardize(llvm::Module&);
	static bool hasNotExpandedCalls(const llvm::Module&, const std::set<std::string>&);
	static bool hasAssertions(const std::vector<LabeledCondition>&);
    static void collectInitBasicBlock(std::vector<int>&);
    static bool basicBlockDFS(std::vector<int>&);
    static int findVerifyLoop(std::vector<LCSSA>&);
    static bool baseCase(std::vector<int>&, int, std::vector<LCSSA>&, unsigned);
};

void printCFGNode();

#endif
