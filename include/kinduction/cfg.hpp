#ifndef KINDUCTION_CFG_HPP
#define KINDUCTION_CFG_HPP

#include "llvm/IR/BasicBlock.h"
#include <vector>

struct CFGNode {
    bool isLoopNode = false;
    int LCSSAIndex = -1;
    llvm::BasicBlock* bb;
    // children's CFGNode index
    std::vector<int> childs;
    CFGNode() {}
    CFGNode(llvm::BasicBlock* bp) { bb = bp; }
    CFGNode(llvm::BasicBlock* bp, bool isLoop) { bb = bp; isLoopNode = isLoop; }
    CFGNode(llvm::BasicBlock* bp, bool isLoop, int idx) { bb = bp; isLoopNode = isLoop; LCSSAIndex = idx; }
    
};


#endif