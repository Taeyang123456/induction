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
#include <llvm/IRReader/IRReader.h>
#include <llvm/Bitcode/BitcodeReader.h>
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

    string fileNameWithoutSuffix = string(argv[1]);
    fileNameWithoutSuffix = fileNameWithoutSuffix.substr(0, fileNameWithoutSuffix.find_last_of('.'));
    

    string cflangs = " -emit-llvm -g -O2 -fno-vectorize -fno-slp-vectorize ";
    string command = "clang -c " + cflangs + argv[1] + " -o " + fileNameWithoutSuffix + ".bc";
    system(command.c_str());

    SMDiagnostic Err;
	unique_ptr<llvm::Module> mod(parseIRFile(fileNameWithoutSuffix + ".bc", Err, g_LLVMCtx));
    cout << "compile finish\n";
    for(unsigned K = 1; K <= 5; K++) {
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
                outs() << "Solution Found by Induction (K = " << K << ")\n"; 
                outs() << "True\n";
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
    ArrayRef<const char *> args{"-O2", "-fno-slp-vectorize", "-fno-vectorize", cfilename.c_str()};
    // Create the compiler invocation
    auto invocation = make_shared<CompilerInvocation>();
	assert(CompilerInvocation::CreateFromArgs(*invocation, args, *DiagEngine));
		
	// Create the compiler instance
    CompilerInstance compiler;
    compiler.setInvocation(invocation);
	compiler.setDiagnostics(DiagEngine.get());
	unique_ptr<CodeGenAction> action = make_unique<EmitLLVMOnlyAction>(&LLVMCtx);
	assert(compiler.ExecuteAction(*action));
	return action->takeModule();	
}

