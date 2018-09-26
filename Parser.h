#ifndef __PARSER_H__
#define __PARSER_H__
#include "Lexer.h"
#include "AST.h"

static int CurTok;
static std::map<char, int> BinopPrecedence;
static int getNextToken() { return CurTok = gettok(); }

static std::unique_ptr<ExprAST> ParseExpression();
std::unique_ptr<ExprAST> LogError(const char *Str);
static std::unique_ptr<ExprAST> ParseNumberExpr();
static std::unique_ptr<ExprAST> ParseParenExpr();
std::unique_ptr<ExprAST> LogError(const char *Str);
std::unique_ptr<PrototypeAST> LogErrorP(const char *Str);

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

	/*auto E = ParseExpression();
	if (!E)
		return nullptr;*/
	if (CurTok != '}')
	{
		LogErrorP("Expected '}' in function");
		return nullptr;
	}
	getNextToken();

	return llvm::make_unique<FunctionAST>(std::move(Proto), nullptr);
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

//������Ϣ��ӡ
std::unique_ptr<ExprAST> LogError(const char *Str) {
	fprintf(stderr, "Error: %s\n", Str);
	return nullptr;
}
std::unique_ptr<PrototypeAST> LogErrorP(const char *Str) {
	LogError(Str);
	return nullptr;
}

// Top-Level parsing
static void HandleFuncDefinition() {
  if (ParseFunc()) {
    fprintf(stderr, "Parsed a function definition.\n");
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

/// toplevelexpr ::= expression
static std::unique_ptr<FunctionAST> ParseTopLevelExpr() {
  if (auto E = ParseExpression()) {
    // Make an anonymous proto.
    auto Proto = llvm::make_unique<PrototypeAST>("__anon_expr",
                                                 std::vector<std::string>());
    return llvm::make_unique<FunctionAST>(std::move(Proto), std::move(E));
  }
  return nullptr;
}

static void HandleTopLevelExpression() {
  // Evaluate a top-level expression into an anonymous function.
  if (ParseTopLevelExpr()) {
    fprintf(stderr, "Parsed a top-level expr\n");
  } else {
    // Skip token for error recovery.
    getNextToken();
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
		default:
			HandleTopLevelExpression();
			break;
	}
	}
}

#endif