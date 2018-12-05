#ifndef __PARSER_H__
#define __PARSER_H__
#include "AST.h"
#include "Lexer.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Verifier.h"
using namespace llvm;

static int CurTok;
static std::map<char, int> BinopPrecedence;
static int getNextToken() { return CurTok = gettok(); }

static std::unique_ptr<ExprAST> ParseExpression();
std::unique_ptr<ExprAST> LogError(const char *Str);
static std::unique_ptr<ExprAST> ParseNumberExpr();
static std::unique_ptr<ExprAST> ParseParenExpr();
static std::unique_ptr<StatAST> ParseDec();
std::unique_ptr<ExprAST> LogError(const char *Str);
std::unique_ptr<PrototypeAST> LogErrorP(const char *Str);
std::unique_ptr<StatAST> LogErrorS(const char *Str);
static std::unique_ptr<StatAST> ParseStatement();

//�������¸�ʽ�ı��ʽ��
// identifer || identifier(expression list)
static std::unique_ptr<ExprAST> ParseIdentifierExpr() {
	std::string IdName = IdentifierStr;

	getNextToken();

	//�����ɱ������ʽ
	if (CurTok != '(')
		return llvm::make_unique<VariableExprAST>(IdName);

	// �����ɺ������ñ��ʽ
	getNextToken();
	std::vector<std::unique_ptr<ExprAST>> Args;
	if (CurTok != ')') {
		while (true) {
			if (auto Arg = ParseExpression())
				Args.push_back(std::move(Arg));
			else
				return nullptr;

			if (CurTok == ')')
				break;

			if (CurTok != ',')
				return LogError("Expected ')' or ',' in argument list");
			getNextToken();
		}
	}

	getNextToken();

	return llvm::make_unique<CallExprAST>(IdName, std::move(Args));
}

//����ȡ�����ʽ
static std::unique_ptr<ExprAST> ParseNegExpr() {
	getNextToken();
	std::unique_ptr<ExprAST> Exp = ParseExpression();
	if (!Exp)
		return nullptr;

	return llvm::make_unique<NegExprAST>(std::move(Exp));
}

//������ ��ʶ�����ʽ���������ʽ�����ű��ʽ�е�һ��
static std::unique_ptr<ExprAST> ParsePrimary() {
	switch (CurTok) {
	default:
		return LogError("unknown token when expecting an expression");
	case VARIABLE:
		return ParseIdentifierExpr();
	case INTEGER:
		return ParseNumberExpr();
	case '(':
		return ParseParenExpr();
	case '-':
		return ParseNegExpr();
	}
}

//GetTokPrecedence - Get the precedence of the pending binary operator token.
static int GetTokPrecedence() {
  if (!isascii(CurTok))
    return -1;

  // Make sure it's a declared binop.
  int TokPrec = BinopPrecedence[CurTok];
  if (TokPrec <= 0)
    return -1;
  return TokPrec;
}

//������Ԫ���ʽ
//���� ��
//ExprPrec ����������ȼ�
//LHS �󲿲�����
// �ݹ�õ����Խ�ϵ��Ҳ���ѭ���õ�һ�������Ԫ���ʽ
static std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec,
	std::unique_ptr<ExprAST> LHS) {

	while (true) {
		int TokPrec = GetTokPrecedence();

		// ���Ҳ�û����������Ҳ���������ȼ�С������������ȼ�ʱ �˳�ѭ���͵ݹ�
		if (TokPrec < ExprPrec)
			return LHS;

		if(CurTok == '}')
			return LHS;

		// �����������
		int BinOp = CurTok;
		getNextToken();

		// �õ��Ҳ����ʽ
		auto RHS = ParsePrimary();
		if (!RHS)
			return nullptr;

		// ������Ҳ����ʽ������󲿱��ʽ��� ��ô�ݹ�õ��Ҳ����ʽ
		int NextPrec = GetTokPrecedence();
		if (TokPrec < NextPrec) {
			RHS = ParseBinOpRHS(TokPrec + 1, std::move(RHS));
			if (!RHS)
				return nullptr;
		}

		// �����Ҳ���ϳ��µ���
		LHS = llvm::make_unique<BinaryExprAST>(BinOp, std::move(LHS),
			std::move(RHS));
	}
}

// �����õ����ʽ
static std::unique_ptr<ExprAST> ParseExpression() {
	auto LHS = ParsePrimary();
	if (!LHS)
		return nullptr;

	return ParseBinOpRHS(0, std::move(LHS));
}

// numberexpr ::= number
static std::unique_ptr<ExprAST> ParseNumberExpr() {
	auto Result = llvm::make_unique<NumberExprAST>(NumberVal);
	//�Թ����ֻ�ȡ��һ������
	getNextToken();
	return std::move(Result);
}

//declaration::=VAR variable_list
static std::unique_ptr<StatAST> ParseDec() {
	//eat 'VAR'
	getNextToken();

	std::vector<std::string> varNames;
	//��֤������һ������������
	if (CurTok != VARIABLE) {
		return LogErrorS("expected identifier after VAR");
	}

	while (true)
	{
		varNames.push_back(IdentifierStr);
		//eat VARIABLE
		getNextToken();
		if (CurTok != ',')
			break;
		getNextToken();
		if (CurTok != VARIABLE) {
			return LogErrorS("expected identifier list after VAR");
		}
	}

	auto Body = nullptr;

	return llvm::make_unique<DecAST>(std::move(varNames), std::move(Body));
}

//prototype ::= VARIABLE '(' parameter_list ')'
static std::unique_ptr<PrototypeAST> ParsePrototype() {
	if (CurTok != VARIABLE)
		return LogErrorP("Expected function name in prototype");

	std::string FnName = IdentifierStr;
	getNextToken();

	if (CurTok != '(')
		return LogErrorP("Expected '(' in prototype");

	std::vector<std::string> ArgNames;
	getNextToken();
	while (CurTok == VARIABLE)
	{
		ArgNames.push_back(IdentifierStr);
		getNextToken();
		if (CurTok == ',')
			getNextToken();
	}
	if (CurTok != ')')
		return LogErrorP("Expected ')' in prototype");

	// success.
	getNextToken(); // eat ')'.

	return llvm::make_unique<PrototypeAST>(FnName, std::move(ArgNames));
}

//function ::= FUNC prototype '{' statement '}'
static std::unique_ptr<FunctionAST> ParseFunc()
{
	getNextToken(); // eat FUNC.
	auto Proto = ParsePrototype();
	if (!Proto)
		return nullptr;
	if (CurTok != '{')
	{
		LogErrorP("Expected '{' in function");
		return nullptr;
	}
	getNextToken();

	auto E = ParseStatement();
	if (!E)
		return nullptr;
	if (CurTok != '}')
	{
		LogErrorP("Expected '}' in function");
		return nullptr;
	}
	getNextToken();

	return llvm::make_unique<FunctionAST>(std::move(Proto), std::move(E));
}

//���������еı��ʽ
static std::unique_ptr<ExprAST> ParseParenExpr() {
	// ����'('
	getNextToken();
	auto V = ParseExpression();
	if (!V)
		return nullptr;

	if (CurTok != ')')
		return LogError("expected ')'");
	// ����')'
	getNextToken();
	return V;
}

//���� IF Statement
static std::unique_ptr<StatAST> ParseIfStat() {
	getNextToken(); // eat the IF.

					// condition.
	auto Cond = ParseExpression();
	if (!Cond)
		return nullptr;

	if (CurTok != THEN)
		return LogErrorS("expected THEN");
	getNextToken(); // eat the THEN

	auto Then = ParseStatement();
	if (!Then)
		return nullptr;

	std::unique_ptr<StatAST> Else = nullptr;
	if (CurTok == ELSE) {
		Else = ParseStatement();
		if (!Else)
			return nullptr;
	}
	else if(CurTok != FI)
		return LogErrorS("expected FI or ELSE");

	getNextToken();

	return llvm::make_unique<IfStatAST>(std::move(Cond), std::move(Then),
		std::move(Else));
}

//���� RETURN Statement
static std::unique_ptr<StatAST> ParseRetStat() {
	getNextToken();
	auto Val = ParseExpression();
	if (!Val)
		return nullptr;

	return llvm::make_unique<RetStatAST>(std::move(Val));
}

//���� ��ֵ���
static std::unique_ptr<StatAST> ParseAssStat() {
	auto a = ParseIdentifierExpr();
	VariableExprAST* Name = (VariableExprAST*)a.get();
	auto NameV = llvm::make_unique<VariableExprAST>(Name->getName());
	if (!Name)
		return nullptr;
	if (CurTok != '=')
		return LogErrorS("need =");
	getNextToken();
	
	auto Expression = ParseExpression();
	if (!Expression)
		return nullptr;

	return llvm::make_unique<AssStatAST>(std::move(NameV), std::move(Expression));
}

static std::unique_ptr<StatAST> ParseStatement() {
	switch (CurTok) {
		case IF:
			return ParseIfStat();
			break;
		case RETURN:
			return ParseRetStat();
		case VAR:
			return ParseDec();
			break;
		default:
			auto E = ParseAssStat();
			return E;
	}
}

//��������ṹ
static std::unique_ptr<ProgramAST> ParseProgramAST() {
	//���ܳ����к������﷨��
	std::vector<std::unique_ptr<FunctionAST>> Functions;

	//ѭ���������������к���
	while (CurTok != TOK_EOF) {
		auto Func=ParseFunc();
		Functions.push_back(std::move(Func));
	}

	return llvm::make_unique<ProgramAST>(std::move(Functions));
}

//������Ϣ��ӡ
std::unique_ptr<ExprAST> LogError(const char *Str) {
	fprintf(stderr, "Error: %s\n", Str);
	return nullptr;
}
std::unique_ptr<PrototypeAST> LogErrorP(const char *Str) {
	LogError(Str);
	return nullptr;
}
std::unique_ptr<StatAST> LogErrorS(const char *Str) {
	fprintf(stderr, "Error: %s\n", Str);
	return nullptr;
}

// Top-Level parsing
static void HandleFuncDefinition() {
	if (auto FnAST = ParseFunc()) {
		if (auto *FnIR = FnAST->codegen()) {
			fprintf(stderr, "Read function definition:");
			FnIR->print(errs());
			fprintf(stderr, "\n");
			//TheJIT->addModule(std::move(TheModule));
			InitializeModuleAndPassManager();
		}
	}
	else {
		// Skip token for error recovery.
		getNextToken();
	}
}

/// toplevelexpr ::= expression
static std::unique_ptr<FunctionAST> ParseTopLevelExpr() {
  if (auto E = ParseStatement()) {
    // Make an anonymous proto.
    auto Proto = llvm::make_unique<PrototypeAST>("__anon_expr",
                                                 std::vector<std::string>());
    return llvm::make_unique<FunctionAST>(std::move(Proto), std::move(E));
  }
  return nullptr;
}

static void HandleTopLevelExpression() {
	// Evaluate a top-level expression into an anonymous function.
	if (auto FnAST = ParseTopLevelExpr()) {
		if (FnAST->codegen()) {

			// JIT the module containing the anonymous expression, keeping a handle so
			// we can free it later.
			auto H = TheJIT->addModule(std::move(TheModule));
			InitializeModuleAndPassManager();

			// Search the JIT for the __anon_expr symbol.
			auto ExprSymbol = TheJIT->findSymbol("__anon_expr");
			assert(ExprSymbol && "Function not found");

			// Get the symbol's address and cast it to the right type (takes no
			// arguments, returns a double) so we can call it as a native function.
			int(*FP)() = (int (*)())cantFail(ExprSymbol.getAddress());
			fprintf(stderr, "Evaluated to %d\n", FP());

			// Delete the anonymous expression module from the JIT.
			TheJIT->removeModule(H);
		}
		else {
			// Skip token for error recovery.
			getNextToken();
		}
	}
}

//program ::= definition | expression
static void MainLoop() {

	while (true) {
		fprintf(stderr, "ready> ");
		switch (CurTok) {
			case ';': // ignore top-level semicolons.
				getNextToken();
				break;
			case TOK_EOF:
				return;
			case FUNC:
		 		HandleFuncDefinition();
				break;
			case VAR:

			default:
				HandleTopLevelExpression();
				break;
		}
	}

}
#endif
