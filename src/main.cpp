#include <cassert>
#include <cstddef>
#include <iostream>
#include "kinduction/k_induction.hpp"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticIDs.h"
#include "clang/Basic/DiagnosticOptions.h"
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include "clang/Frontend/CompilerInvocation.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "llvm/IR/LLVMContext.h"
#include <llvm/IR/Module.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/CodeGen/CodeGenAction.h>
#include <memory>
using namespace std;
using namespace llvm;
using namespace clang;

static unique_ptr<llvm::Module> compile(const string& cfilename, LLVMContext&);

static LLVMContext g_LLVMCtx;
int main(int argc ,char* argv[]){
	if(argc < 2){
		cerr<<"No input file!"<<endl<<"This executable file takes a '.c' filepath as its argument"<<endl;
		return 0;
	}
	unique_ptr<llvm::Module> mod = compile(argv[1], g_LLVMCtx);
    for(unsigned K = 1; K <= 5; K+=2) {
        auto result = KInduction::verify(*mod, K);
        switch (result) {
            case KInduction::UNKNOWN :
                outs()<<"Unknown\n";
                break;
            
            case KInduction::FALSE :
                outs()<<"False\n";
                return 0;
                break;

            case KInduction::TRUE :
                outs()<<"True\n";
                return 0;
                break;
            
            default:
                break;
        }
    }
}


unique_ptr<llvm::Module> compile(const string& cfilename, LLVMContext& LLVMCtx){
	outs()<<"+=========== Compiling ===========+\n";
	outs()<<"Input: "<<cfilename<<"\n";
	outs()<<"Output: ";
	IntrusiveRefCntPtr<DiagnosticOptions> DiagOpts(new DiagnosticOptions());
	IntrusiveRefCntPtr<DiagnosticIDs> DiagIDs(new DiagnosticIDs());
	IntrusiveRefCntPtr<DiagnosticsEngine> DiagEngine(new DiagnosticsEngine(DiagIDs,DiagOpts,new TextDiagnosticPrinter(outs(),DiagOpts.get())));


	// Arguments to pass to the clang frontend
    ArrayRef<const char *> args{"-O2",cfilename.c_str()};
    // Create the compiler invocation
    auto invocation = make_shared<CompilerInvocation>();
	assert(CompilerInvocation::CreateFromArgs(*invocation, args,*DiagEngine));
		
	// Create the compiler instance
    CompilerInstance compiler;
    compiler.setInvocation(invocation);
	compiler.setDiagnostics(DiagEngine.get());
	unique_ptr<CodeGenAction> action = make_unique<EmitLLVMOnlyAction>(&LLVMCtx);
	assert(compiler.ExecuteAction(*action));
	return action->takeModule();	
}

