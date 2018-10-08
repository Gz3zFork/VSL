#include "Lexer.h"
#include "AST.h"
#include "Parser.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Verifier.h"
using namespace llvm;

//report errors found during code generation
Value *LogErrorV(const char *Str) {
	LogError(Str);
	return nullptr;
}

Value *NumberExprAST::codegen() {
	return ConstantFP::get(TheContext, APFloat((double)Val));
}

Value *VariableExprAST::codegen() {
	// Look this variable up in the function.
	Value *V = NamedValues[Name];
	if (!V)
		return LogErrorV("Unknown variable name");
	return V;
}

Value *BinaryExprAST::codegen() {
	Value *L = LHS->codegen();
	Value *R = RHS->codegen();
	if (!L || !R)
		return nullptr;

	switch (Op) {
	case '+':
		return Builder.CreateFAdd(L, R, "addtmp");
	case '-':
		return Builder.CreateFSub(L, R, "subtmp");
	case '*':
		return Builder.CreateFMul(L, R, "multmp");
	case '/':
		L = Builder.CreateFDiv(L, R, "divtmp");
		// Convert bool 0/1 to double 0.0 or 1.0
		return Builder.CreateUIToFP(L, Type::getDoubleTy(TheContext), "booltmp");
	default:
		return LogErrorV("invalid binary operator");
	}
}

Value *CallExprAST::codegen() {
	// Look up the name in the global module table.
	Function *CalleeF = TheModule->getFunction(Callee);
	if (!CalleeF)
		return LogErrorV("Unknown function referenced");

	// If argument mismatch error.
	if (CalleeF->arg_size() != Args.size())
		return LogErrorV("Incorrect # arguments passed");

	std::vector<Value *> ArgsV;
	for (unsigned i = 0, e = Args.size(); i != e; ++i) {
		ArgsV.push_back(Args[i]->codegen());
		if (!ArgsV.back())
			return nullptr;
	}

	return Builder.CreateCall(CalleeF, ArgsV, "calltmp");
}

/*
 * ���ع���VSL �в���Ҫ PrototypeAST
 * VSL �еĺ�����������Ҫ�������壬����û�б�Ҫ�� Prototype �������
*/
Function *PrototypeAST::codegen() {
	//���������ض���
	Function *TheFunction = TheModule->getFunction(Name);
	if(TheFunction)
		return (Function*)LogErrorV("Function cannot be redefined.");

	// �����β�����Ϊ int
	std::vector<Type*> Integers(Args.size(),
		Type::getInt32Ty(TheContext));
	FunctionType *FT =
		FunctionType::get(Type::getInt32Ty(TheContext), Integers, false);

	// ע��ú���
	Function *F =
		Function::Create(FT, Function::InternalLinkage, Name, TheModule);

	// Ϊ������������
	unsigned Idx = 0;
	for (auto &Arg : F->args())
		Arg.setName(Args[Idx++]);

	return F;
}

/*
 * ���ع������� 
*/
Function *FunctionAST::codegen() {
	// ���� Proto �� codegen()
	Function *TheFunction = Proto->codegen();

	if (!TheFunction)
		return nullptr;

	// Create a new basic block to start insertion into.
	BasicBlock *BB = BasicBlock::Create(TheContext, "entry", TheFunction);
	Builder.SetInsertPoint(BB);

	// Record the function arguments in the NamedValues map.
	NamedValues.clear();
	for (auto &Arg : TheFunction->args())
		NamedValues[Arg.getName()] = &Arg;

	if (Value *RetVal = Body->codegen()) {
		// Finish off the function.
		Builder.CreateRet(RetVal);

		// Validate the generated code, checking for consistency.
		verifyFunction(*TheFunction);

		return TheFunction;
	}

	// Error reading body, remove function.
	TheFunction->eraseFromParent();
	return nullptr;
}