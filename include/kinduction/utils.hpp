#ifndef KINDUCTION_UTILS_H
#define KINDUCTION_UTILS_H

#include "kinduction/defs.hpp"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include <map>
#include <set>

void findBranchPoints(const llvm::BasicBlock*,std::set<std::pair<const llvm::BasicBlock*,bool>>&);

#endif
