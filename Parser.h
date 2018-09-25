#include "Lexer.h"
#include "AST.h"
#include "llvm/ADT/STLExtras.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>

static int CurTok;
static int getNextToken() { return CurTok = gettok(); }

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
	case tok_identifier:
		return ParseIdentifierExpr();
	case tok_number:
		return ParseNumberExpr();
	case '(':
		return ParseParenExpr();
	}
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