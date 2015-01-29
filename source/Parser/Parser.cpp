// Parser.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <map>
#include <deque>
#include <cfloat>
#include <fstream>
#include <cassert>
#include <inttypes.h>
#include "../include/ast.h"
#include "../include/parser.h"
#include "../include/compiler.h"

using namespace Ast;


namespace Parser
{
	PosInfo currentPos;
	Root* rootNode;
	Token* curtok;
	uint32_t curAttrib;
	Codegen::CodegenInstance* curCgi;


	#define CreateAST_Raw(name, ...)		(new name (currentPos, ##__VA_ARGS__))
	#define CreateAST(name, tok, ...)		(new name (tok->posinfo, ##__VA_ARGS__))





	// todo: hack
	bool isParsingStruct;
	void error(const char* msg, ...)
	{
		va_list ap;
		va_start(ap, msg);

		char* alloc = nullptr;
		vasprintf(&alloc, msg, ap);

		fprintf(stderr, "%s(%s:%" PRId64 ")%s Parsing error%s: %s\n\n", COLOUR_BLACK_BOLD, curtok->posinfo.file.c_str(), curtok->posinfo.line, COLOUR_RED_BOLD, COLOUR_RESET, alloc);

		va_end(ap);
		exit(1);
	}

	void warn(const char* msg, ...)
	{
		if(Compiler::getFlag(Compiler::Flag::NoWarnings))
			return;

		va_list ap;
		va_start(ap, msg);

		char* alloc = nullptr;
		vasprintf(&alloc, msg, ap);

		fprintf(stderr, "%s(%s:%" PRId64 ")%s Warning%s: %s\n\n", COLOUR_BLACK_BOLD, curtok->posinfo.file.c_str(), curtok->posinfo.line, COLOUR_MAGENTA_BOLD, COLOUR_RESET, alloc);

		va_end(ap);

		if(Compiler::getFlag(Compiler::Flag::WarningsAsErrors))
			error("Treating warning as error because -Werror was passed");
	}


	// woah shit it's forward declarations
	// note: all these are expected to pop at least one token from the front of the list.

	Expr* parseIf(std::deque<Token*>& tokens);
	void parseAll(std::deque<Token*>& tokens);
	Func* parseFunc(std::deque<Token*>& tokens);
	Expr* parseExpr(std::deque<Token*>& tokens);
	Expr* parseUnary(std::deque<Token*>& tokens);
	ForLoop* parseFor(std::deque<Token*>& tokens);
	Expr* parseIdExpr(std::deque<Token*>& tokens);
	Break* parseBreak(std::deque<Token*>& tokens);
	Expr* parsePrimary(std::deque<Token*>& tokens);
	Struct* parseStruct(std::deque<Token*>& tokens);
	Import* parseImport(std::deque<Token*>& tokens);
	Return* parseReturn(std::deque<Token*>& tokens);
	Number* parseNumber(std::deque<Token*>& tokens);
	void parseAttribute(std::deque<Token*>& tokens);
	CastedType* parseType(std::deque<Token*>& tokens);
	VarDecl* parseVarDecl(std::deque<Token*>& tokens);
	Closure* parseClosure(std::deque<Token*>& tokens);
	WhileLoop* parseWhile(std::deque<Token*>& tokens);
	Continue* parseContinue(std::deque<Token*>& tokens);
	Func* parseTopLevelExpr(std::deque<Token*>& tokens);
	FuncDecl* parseFuncDecl(std::deque<Token*>& tokens);
	Expr* parseParenthesised(std::deque<Token*>& tokens);
	OpOverload* parseOpOverload(std::deque<Token*>& tokens);
	InfiniteLoop* parseInfiniteLoop(std::deque<Token*>& tokens);
	StringLiteral* parseStringLiteral(std::deque<Token*>& tokens);
	ForeignFuncDecl* parseForeignFunc(std::deque<Token*>& tokens);
	Expr* parseRhs(std::deque<Token*>& tokens, Expr* expr, int prio, bool needParen);
	Expr* parseFunctionCall(std::deque<Token*>& tokens, std::string id);


	std::string getModuleName(std::string filename)
	{
		size_t lastdot = filename.find_last_of(".");
		std::string modname = (lastdot == std::string::npos ? filename : filename.substr(0, lastdot));

		size_t sep = modname.find_last_of("\\/");
		if(sep != std::string::npos)
			modname = modname.substr(sep + 1, modname.length() - sep - 1);

		return modname;
	}

	Root* Parse(std::string filename, std::string str, Codegen::CodegenInstance* cgi)
	{
		curCgi = cgi;

		Token* t = nullptr;
		currentPos.file = filename;
		currentPos.line = 1;

		std::deque<Token*> tokens;

		while((t = getNextToken(str, currentPos)) != nullptr)
			tokens.push_back(t);

		rootNode = new Root();
		currentPos.file = filename;
		currentPos.line = 1;

		parseAll(tokens);

		return rootNode;
	}

	// helpers
	static void skipNewline(std::deque<Token*>& tokens)
	{
		while(tokens.size() > 0 && tokens.front()->type == TType::NewLine)
		{
			tokens.pop_front();
			currentPos.line++;
		}
	}

	static Token* eat(std::deque<Token*>& tokens)
	{
		// returns the current front, then pops front.
		if(tokens.size() == 0)
			error("Unexpected end of input");

		Token* t = tokens.front();
		tokens.pop_front();
		skipNewline(tokens);

		curtok = t;
		return t;
	}

	static bool checkHasMore(std::deque<Token*>& tokens)
	{
		return tokens.size() > 0;
	}

	static int getOpPrec(Token* tok)
	{
		if(!tok)
			return -1;

		switch(tok->type)
		{
			case TType::As:
				return 200;

			case TType::DoublePlus:
			case TType::DoubleMinus:
				return 100;

			case TType::Asterisk:
			case TType::Divide:
			case TType::Percent:
				return 90;

			case TType::Plus:
			case TType::Minus:
				return 80;

			case TType::ShiftLeft:
			case TType::ShiftRight:
				return 70;

			case TType::LAngle:
			case TType::RAngle:
			case TType::LessThanEquals:
			case TType::GreaterEquals:
				return 60;

			case TType::EqualsTo:
			case TType::NotEquals:
				return 50;

			case TType::Ampersand:
				return 30;

			case TType::Pipe:
				return 25;

			case TType::LogicalAnd:
				return 20;

			case TType::LogicalOr:
				return 15;

			case TType::Equal:
				return 10;

			default:
				return -1;
		}
	}

	VarType determineVarType(std::string type_id)
	{
		// kinda hardcoded
		if(type_id == "Int8")			return VarType::Int8;
		else if(type_id == "Int16")		return VarType::Int16;
		else if(type_id == "Int32")		return VarType::Int32;
		else if(type_id == "Int64")		return VarType::Int64;
		else if(type_id == "Uint8")		return VarType::Uint8;
		else if(type_id == "Uint16")	return VarType::Uint16;
		else if(type_id == "Uint32")	return VarType::Uint32;
		else if(type_id == "Uint64")	return VarType::Uint64;

		else if(type_id == "AnyPtr")	return VarType::AnyPtr;

		else if(type_id == "Float32")	return VarType::Float32;
		else if(type_id == "Float64")	return VarType::Float64;
		else if(type_id == "Bool")		return VarType::Bool;
		else if(type_id == "Void")		return VarType::Void;
		else
		{
			// todo: risky
			if(type_id.back() == ']')
				return VarType::Array;

			else
				return VarType::UserDefined;
		}
	}

	int64_t getIntegerValue(Token* t)
	{
		assert(t->type == TType::Integer);
		int base = 10;
		if(t->text.find("0x") == 0)
			base = 16;

		return std::stoll(t->text, nullptr, base);
	}










	void parseAll(std::deque<Token*>& tokens)
	{
		if(tokens.size() == 0)
			return;

		Token* tok = nullptr;
		while(tokens.size() > 0 && (tok = tokens.front()))
		{
			assert(tok != nullptr);
			switch(tok->type)
			{
				case TType::Func:
					rootNode->functions.push_back(parseFunc(tokens));
					break;

				case TType::Import:
					rootNode->imports.push_back(parseImport(tokens));
					break;

				case TType::ForeignFunc:
					rootNode->foreignfuncs.push_back(parseForeignFunc(tokens));
					break;

				case TType::Struct:
					rootNode->structs.push_back(parseStruct(tokens));
					break;

				// shit you just skip
				case TType::NewLine:
					currentPos.line++;
					// no break

				case TType::Comment:
				case TType::Semicolon:
					tokens.pop_front();
					break;

				case TType::Private:
					eat(tokens);
					curAttrib |= Attr_VisPrivate;
					break;

				case TType::Internal:
					eat(tokens);
					curAttrib |= Attr_VisInternal;
					break;

				case TType::Public:
					eat(tokens);
					curAttrib |= Attr_VisPublic;
					break;

				case TType::At:
					parseAttribute(tokens);
					break;

				default:	// wip: skip shit we don't know/care about for now
					parseTopLevelExpr(tokens);
					break;
			}
		}
	}

	Func* parseTopLevelExpr(std::deque<Token*>& tokens)
	{
		Expr* expr = parseExpr(tokens);
		FuncDecl* fakedecl = CreateAST_Raw(FuncDecl, "__anonymous_toplevel_0", std::deque<VarDecl*>(), "");
		Closure* cl = CreateAST_Raw(Closure);
		cl->statements.push_back(expr);

		Func* fakefunc = CreateAST_Raw(Func, fakedecl, cl);
		rootNode->functions.push_back(fakefunc);

		return fakefunc;
	}

	Expr* parseUnary(std::deque<Token*>& tokens)
	{
		Token* front = tokens.front();

		// check for unary shit
		if(front->type == TType::Exclamation || front->type == TType::Plus || front->type == TType::Minus)
		{
			TType tp = eat(tokens)->type;
			ArithmeticOp op = tp == TType::Exclamation ? ArithmeticOp::LogicalNot : (tp == TType::Plus ? ArithmeticOp::Plus : ArithmeticOp::Minus);

			return CreateAST(UnaryOp, front, op, parseUnary(tokens));
		}
		else if(front->type == TType::Deref || front->type == TType::Pound)
		{
			eat(tokens);
			return CreateAST(UnaryOp, front, ArithmeticOp::Deref, parseUnary(tokens));
		}
		else if(front->type == TType::Addr || front->type == TType::Ampersand)
		{
			eat(tokens);
			return CreateAST(UnaryOp, front, ArithmeticOp::AddrOf, parseUnary(tokens));
		}
		else
		{
			return parsePrimary(tokens);
		}
	}

	Expr* parsePrimary(std::deque<Token*>& tokens)
	{
		if(tokens.size() == 0)
			return nullptr;

		while(Token* tok = tokens.front())
		{
			assert(tok != nullptr);
			switch(tok->type)
			{
				case TType::Var:
				case TType::Val:
					return parseVarDecl(tokens);

				case TType::Func:
					return parseFunc(tokens);

				case TType::LParen:
					return parseParenthesised(tokens);

				case TType::BuiltinType:
				case TType::Identifier:
					if(tok->text == "init")
						return parseFunc(tokens);

					else if(tok->text == "operator")
						return parseOpOverload(tokens);

					return parseIdExpr(tokens);

				case TType::At:
					parseAttribute(tokens);
					return parsePrimary(tokens);

				case TType::StringLiteral:
					return parseStringLiteral(tokens);

				case TType::Integer:
				case TType::Decimal:
					return parseNumber(tokens);

				case TType::Return:
					return parseReturn(tokens);

				case TType::Break:
					return parseBreak(tokens);

				case TType::Continue:
					return parseContinue(tokens);

				case TType::If:
					return parseIf(tokens);

				// since both have the same kind of AST node, parseWhile can handle both
				case TType::Do:
				case TType::While:
					return parseWhile(tokens);

				case TType::Loop:
					return parseInfiniteLoop(tokens);

				case TType::For:
					return parseFor(tokens);

				// shit you just skip
				case TType::NewLine:
					currentPos.line++;
					// fallthrough

				case TType::Comment:
				case TType::Semicolon:
					eat(tokens);
					return CreateAST(DummyExpr, tok);

				case TType::True:
					tokens.pop_front();
					return CreateAST(BoolVal, tok, true);

				case TType::False:
					tokens.pop_front();
					return CreateAST(BoolVal, tok, false);


				case TType::Private:
					eat(tokens);
					curAttrib |= Attr_VisPrivate;
					return parsePrimary(tokens);

				case TType::Internal:
					eat(tokens);
					curAttrib |= Attr_VisInternal;
					return parsePrimary(tokens);

				case TType::Public:
					eat(tokens);
					curAttrib |= Attr_VisPublic;
					return parsePrimary(tokens);

				case TType::LBrace:
					warn("Anonymous closures are ignored; to run, preface with 'do'");
					parseClosure(tokens);		// parse it, but throw it away
					return CreateAST(DummyExpr, tokens.front());

				default:
					error("Unexpected token '%s'\n", tok->text.c_str());
					break;
			}
		}

		return nullptr;
	}





	FuncDecl* parseFuncDecl(std::deque<Token*>& tokens)
	{
		// todo: better things? it's right now mostly hacks.
		if(tokens.front()->text != "init" && tokens.front()->text.find("operator") != 0)
			assert(eat(tokens)->type == TType::Func);

		if(tokens.front()->type != TType::Identifier)
			error("Expected identifier, but got token of type %d", tokens.front()->type);

		Token* func_id = eat(tokens);
		std::string id = func_id->text;

		// expect a left bracket
		Token* paren = eat(tokens);
		if(paren->type != TType::LParen)
			error("Expected '(' in function declaration, got '%s'", paren->text.c_str());

		bool isVA = false;
		// get the parameter list
		// expect an identifer, colon, type
		std::deque<VarDecl*> params;
		std::map<std::string, VarDecl*> nameCheck;
		while(tokens.size() > 0 && tokens.front()->type != TType::RParen)
		{
			Token* tok_id;
			if((tok_id = eat(tokens))->type != TType::Identifier)
			{
				if(tok_id->type == TType::Elipsis)
				{
					isVA = true;
					if(tokens.front()->type != TType::RParen)
						error("Vararg declaration must be last in the function declaration");

					break;
				}
				else
				{
					error("Expected identifier");
				}
			}

			std::string id = tok_id->text;
			VarDecl* v = CreateAST(VarDecl, tok_id, id, true);

			// expect a colon
			if(eat(tokens)->type != TType::Colon)
				error("Expected ':' followed by a type");

			v->type = parseType(tokens)->name;
			v->varType = determineVarType(v->type);

			if(!nameCheck[v->name])
			{
				params.push_back(v);
				nameCheck[v->name] = v;
			}
			else
			{
				error("Redeclared variable '%s' in argument list", v->name.c_str());
			}

			if(tokens.front()->type == TType::Comma)
				eat(tokens);
		}

		// consume the closing paren
		eat(tokens);

		// get return type.
		std::string ret;
		Token* tok_type = nullptr;
		if(checkHasMore(tokens) && tokens.front()->type == TType::Arrow)
		{
			eat(tokens);
			ret = parseType(tokens)->name;
		}
		else
		{
			ret = "Void";
		}

		skipNewline(tokens);
		FuncDecl* f = CreateAST(FuncDecl, func_id, id, params, ret);
		f->attribs = curAttrib;
		curAttrib = 0;

		f->hasVarArg = isVA;
		f->varType = tok_type == nullptr ? VarType::Void : determineVarType(tok_type->text);

		return f;
	}

	ForeignFuncDecl* parseForeignFunc(std::deque<Token*>& tokens)
	{
		Token* func = tokens.front();
		assert(func->type == TType::ForeignFunc);
		eat(tokens);

		FuncDecl* decl = parseFuncDecl(tokens);
		decl->isFFI = true;

		return CreateAST(ForeignFuncDecl, func, decl);
	}

	Closure* parseClosure(std::deque<Token*>& tokens)
	{
		Token* tok_cls = eat(tokens);
		Closure* c = CreateAST(Closure, tok_cls);

		// make sure the first token is a left brace.
		if(tok_cls->type != TType::LBrace)
			error("Expected '{' to begin a block, found '%s'!", tok_cls->text.c_str());

		// get the stuff inside.
		while(tokens.size() > 0 && tokens.front()->type != TType::RBrace)
		{
			c->statements.push_back(parseExpr(tokens));
			skipNewline(tokens);
		}

		if(eat(tokens)->type != TType::RBrace)
			error("Expected '}'");

		return c;
	}

	Func* parseFunc(std::deque<Token*>& tokens)
	{
		Token* front = tokens.front();
		FuncDecl* decl = parseFuncDecl(tokens);

		return CreateAST(Func, front, decl, parseClosure(tokens));
	}



















	CastedType* parseType(std::deque<Token*>& tokens)
	{
		bool isArr = false;
		int arrsize = 0;
		Token* tmp = eat(tokens);
		if(tmp->type != TType::Identifier && tmp->type != TType::BuiltinType)
			error("Expected type for variable declaration");

		std::string ptrAppend = "";
		if(tokens.size() > 0)
		{
			if(tokens.front()->type == TType::Ptr || tokens.front()->type == TType::Asterisk)
			{
				while(tokens.front()->type == TType::Ptr || tokens.front()->type == TType::Asterisk)
					eat(tokens), ptrAppend += "Ptr";
			}
			else if(tokens.front()->type == TType::LSquare)
			{
				isArr = true;
				eat(tokens);

				Token* next = eat(tokens);
				if(next->type == TType::Integer)
					arrsize = getIntegerValue(next), next = eat(tokens);

				if(next->type != TType::RSquare)
					error("Expected either constant integer or ']' after array declaration and '['");
			}
		}

		std::string ret = tmp->text + ptrAppend + (isArr ? "[" + std::to_string(arrsize) + "]" : "");
		return CreateAST(CastedType, tmp, ret);
	}

	VarDecl* parseVarDecl(std::deque<Token*>& tokens)
	{
		assert(tokens.front()->type == TType::Var || tokens.front()->type == TType::Val);

		bool immutable = tokens.front()->type == TType::Val;
		eat(tokens);

		// get the identifier.
		Token* tok_id;
		if((tok_id = eat(tokens))->type != TType::Identifier)
			error("Expected identifier for variable declaration.");

		std::string id = tok_id->text;
		VarDecl* v = CreateAST(VarDecl, tok_id, id, immutable);

		// check the type.
		// todo: type inference
		if(eat(tokens)->type != TType::Colon)
			error("Expected colon to indicate type for variable declaration");

		v->type = parseType(tokens)->name;
		v->varType = determineVarType(v->type);

		// TODO:
		// check if we have a default value
		v->initVal = nullptr;
		if(tokens.front()->type == TType::Equal)
		{
			// we do
			eat(tokens);

			v->initVal = parseExpr(tokens);
			if(!v->initVal)
				error("Invalid initialiser for variable '%s'", v->name.c_str());
		}

		return v;
	}

	Expr* parseParenthesised(std::deque<Token*>& tokens)
	{
		assert(eat(tokens)->type == TType::LParen);
		Expr* within = parseExpr(tokens);

		if(eat(tokens)->type != TType::RParen)
			error("Expected ')'");

		return within;
	}

	Expr* parseExpr(std::deque<Token*>& tokens)
	{
		bool hadParen = false;

		// optional parenthesis handling
		if(tokens.front()->type == TType::LParen)
		{
			eat(tokens);
			hadParen = true;
		}

		Expr* lhs = parseUnary(tokens);
		if(!lhs)
			return nullptr;

		Expr* ret = parseRhs(tokens, lhs, 0, hadParen);
		if(hadParen && !dynamic_cast<BinOp*>(ret))
		{
			if(tokens.front()->type != TType::RParen)
				error("Expected ')'");

			eat(tokens);
		}

		return ret;
	}

	Expr* parseRhs(std::deque<Token*>& tokens, Expr* lhs, int prio, bool needParen)
	{
		while(true)
		{
			int prec = getOpPrec(tokens.front());
			if(prec < prio)
				return lhs;


			// we don't really need to check, because if it's botched we'll have returned due to -1 < everything
			Token* tok_op = eat(tokens);

			Expr* rhs = tok_op->type == TType::As ? parseType(tokens) : parseUnary(tokens);
			if(needParen && tokens.front()->type != TType::RParen)
				error("Expected ')'");


			if(needParen)
			{
				eat(tokens);
				needParen = false;
			}


			if(!rhs)
				return nullptr;

			int next = getOpPrec(tokens.front());
			if(prec < next)
			{
				rhs = parseRhs(tokens, rhs, prec + 1, needParen);
				if(!rhs)
					return nullptr;
			}

			ArithmeticOp op;
			switch(tok_op->type)
			{
				case TType::Plus:			op = ArithmeticOp::Add;			break;
				case TType::Minus:			op = ArithmeticOp::Subtract;	break;
				case TType::Asterisk:		op = ArithmeticOp::Multiply;	break;
				case TType::Divide:			op = ArithmeticOp::Divide;		break;
				case TType::Percent:		op = ArithmeticOp::Modulo;		break;
				case TType::ShiftLeft:		op = ArithmeticOp::ShiftLeft;	break;
				case TType::ShiftRight:		op = ArithmeticOp::ShiftRight;	break;
				case TType::Equal:			op = ArithmeticOp::Assign;		break;

				case TType::LAngle:			op = ArithmeticOp::CmpLT;		break;
				case TType::RAngle:			op = ArithmeticOp::CmpGT;		break;
				case TType::LessThanEquals:	op = ArithmeticOp::CmpLEq;		break;
				case TType::GreaterEquals:	op = ArithmeticOp::CmpGEq;		break;
				case TType::EqualsTo:		op = ArithmeticOp::CmpEq;		break;
				case TType::NotEquals:		op = ArithmeticOp::CmpNEq;		break;

				case TType::Ampersand:		op = ArithmeticOp::BitwiseAnd;	break;
				case TType::Pipe:			op = ArithmeticOp::BitwiseOr;	break;
				case TType::LogicalOr:		op = ArithmeticOp::LogicalOr;	break;
				case TType::LogicalAnd:		op = ArithmeticOp::LogicalAnd;	break;
				case TType::As:				op = ArithmeticOp::Cast;		break;
				default:					error("Unknown operator '%s'", tok_op->text.c_str());
			}

			lhs = CreateAST(BinOp, tok_op, lhs, op, rhs);
		}
	}

	Expr* parseIdExpr(std::deque<Token*>& tokens)
	{
		Token* tok_id = eat(tokens);
		std::string id = tok_id->text;
		VarRef* idvr = CreateAST(VarRef, tok_id, id);

		// check for dot syntax.
		if(tokens.front()->type == TType::Period)
		{
			eat(tokens);
			if(tokens.front()->type != TType::Identifier)
				error("Expected identifier after '.' operator");

			return CreateAST(MemberAccess, tok_id, idvr, parseIdExpr(tokens));
		}
		else if(tokens.front()->type == TType::LSquare)
		{
			// array dereference
			eat(tokens);
			Expr* within = parseExpr(tokens);

			if(eat(tokens)->type != TType::RSquare)
				error("Expected ']'");

			return CreateAST(ArrayIndex, tok_id, idvr, within);
		}
		else if(tokens.front()->type == TType::Ptr || tokens.front()->type == TType::Asterisk)
		{
			eat(tokens);
			idvr->name += "Ptr";
			return idvr;
		}
		else if(tokens.front()->type != TType::LParen)
		{
			return idvr;
		}
		else
		{
			delete idvr;
			return parseFunctionCall(tokens, id);
		}
	}

	Number* parseNumber(std::deque<Token*>& tokens)
	{
		Number* n;
		if(tokens.front()->type == TType::Integer)
		{
			Token* tok = eat(tokens);
			n = CreateAST(Number, tok, getIntegerValue(tok));

			// set the type.
			// always used signed
			n->varType = VarType::Int64;
		}
		else if(tokens.front()->type == TType::Decimal)
		{
			Token* tok = eat(tokens);
			n = CreateAST(Number, tok, std::stod(tok->text));

			if(n->dval < FLT_MAX)	n->varType = VarType::Float32;
			else					n->varType = VarType::Float64;
		}
		else
		{
			error("What!????");
			assert(false);
			return nullptr;
		}

		return n;
	}

	Expr* parseFunctionCall(std::deque<Token*>& tokens, std::string id)
	{
		Token* front = eat(tokens);
		assert(front->type == TType::LParen);

		std::deque<Expr*> args;
		if(tokens.front()->type != TType::RParen)
		{
			while(true)
			{
				Expr* arg = parseExpr(tokens);
				if(arg == nullptr)
					return nullptr;

				args.push_back(arg);
				if(tokens.front()->type == TType::RParen)
				{
					eat(tokens);
					break;
				}

				Token* t;
				if((t = eat(tokens))->type != TType::Comma)
					error("Expected either ',' or ')' in parameter list, got '%s'", t->text.c_str());
			}
		}
		else
		{
			eat(tokens);
		}

		return CreateAST(FuncCall, front, id, args);
	}

	Return* parseReturn(std::deque<Token*>& tokens)
	{
		Token* front = eat(tokens);
		assert(front->type == TType::Return);

		return CreateAST(Return, front, parseExpr(tokens));
	}

	Expr* parseIf(std::deque<Token*>& tokens)
	{
		Token* tok_if = eat(tokens);
		assert(tok_if->type == TType::If);

		typedef std::pair<Expr*, Closure*> CCPair;
		std::deque<CCPair> conds;

		Expr* cond = parseExpr(tokens);
		Closure* tcase = parseClosure(tokens);

		conds.push_back(CCPair(cond, tcase));

		// check for else and else if
		Closure* ecase = nullptr;
		bool parsedElse = false;
		while(tokens.front()->type == TType::Else)
		{
			eat(tokens);
			if(tokens.front()->type == TType::If && !parsedElse)
			{
				eat(tokens);

				// parse an expr, then a closure
				Expr* c = parseExpr(tokens);
				Closure* cl = parseClosure(tokens);

				conds.push_back(CCPair(c, cl));
			}
			else if(!parsedElse)
			{
				parsedElse = true;
				ecase = parseClosure(tokens);
			}
			else
			{
				if(parsedElse && tokens.front()->type != TType::If)
				{
					error("Duplicate 'else' clause, only one else clause is permitted per if.");
				}
				else
				{
					error("The 'else' clause must be the last block in the if statement.");
				}
			}
		}

		return CreateAST(If, tok_if, conds, ecase);
	}

	WhileLoop* parseWhile(std::deque<Token*>& tokens)
	{
		Token* tok_while = eat(tokens);

		if(tok_while->type == TType::While)
		{
			Expr* cond = parseExpr(tokens);
			Closure* body = parseClosure(tokens);

			return CreateAST(WhileLoop, tok_while, cond, body, false);
		}
		else
		{
			assert(tok_while->type == TType::Do);

			// parse the closure first
			Closure* body = parseClosure(tokens);

			// syntax treat: since a raw closure is ignored (for good reason, how can we reference it?)
			// we can use 'do' to run an anonymous closure
			// therefore, the 'while' clause at the end is optional; if it's not present, then the condition is false.

			Expr* cond = 0;
			if(tokens.front()->type == TType::While)
			{
				eat(tokens);
				cond = parseExpr(tokens);
			}
			else
			{
				cond = CreateAST(BoolVal, tokens.front(), false);
			}

			return CreateAST(WhileLoop, tok_while, cond, body, true);
		}
	}

	InfiniteLoop* parseInfiniteLoop(std::deque<Token*>& tokens)
	{
		return 0;
	}

	ForLoop* parseFor(std::deque<Token*>& tokens)
	{
		Token* tok_for = eat(tokens);
		assert(tok_for->type == TType::For);

		return 0;
	}

	Struct* parseStruct(std::deque<Token*>& tokens)
	{
		Token* tok_struct = eat(tokens);
		assert(tok_struct->type == TType::Struct);
		isParsingStruct = true;

		// get the identifier (name)
		std::string id;
		if(tokens.front()->type != TType::Identifier)
			error("Expected name after 'struct'");

		id += eat(tokens)->text;
		Struct* str = CreateAST(Struct, tok_struct, id);

		// parse a clousure.
		Closure* body = parseClosure(tokens);
		int i = 0;
		for(Expr* stmt : body->statements)
		{
			// check for top-level statements
			VarDecl* var = nullptr;
			Func* func = nullptr;
			OpOverload* oo = nullptr;

			if((var = dynamic_cast<VarDecl*>(stmt)))
			{
				if(str->nameMap.find(var->name) != str->nameMap.end())
					error("Duplicate member '%s'", var->name.c_str());

				str->members.push_back(var);
				str->nameMap[var->name] = i;
			}
			else if((func = dynamic_cast<Func*>(stmt)))
			{
				str->funcs.push_back(func);
				str->typeList.push_back(std::pair<Expr*, int>(func, i));
			}
			else if((oo = dynamic_cast<OpOverload*>(stmt)))
			{
				oo->str = str;
				str->opmap[oo->op] = oo;

				str->funcs.push_back(oo->func);
				str->typeList.push_back(std::pair<Expr*, int>(oo, i));
			}
			else if(dynamic_cast<DummyExpr*>(stmt))
			{
				continue;
			}
			else
			{
				error("Only variable and function declarations are allowed in structs");
			}

			i++;
		}

		return str;
	}

	void parseAttribute(std::deque<Token*>& tokens)
	{
		assert(eat(tokens)->type == TType::At);
		Token* id = eat(tokens);

		if(id->type != TType::Identifier)
			error("Expected attribute name after '@'");

		uint32_t attr;
		if(id->text == "nomangle")				attr |= Attr_NoMangle;
		else if(id->text == "forcemangle")		attr |= Attr_ForceMangle;
		else									error("Unknown attribute '%s'", id->text.c_str());

		curAttrib |= attr;
	}

	Break* parseBreak(std::deque<Token*>& tokens)
	{
		Token* tok_br = eat(tokens);
		assert(tok_br->type == TType::Break);

		Break* br = CreateAST(Break, tok_br);
		return br;
	}

	Continue* parseContinue(std::deque<Token*>& tokens)
	{
		Token* tok_cn = eat(tokens);
		assert(tok_cn->type == TType::Continue);

		Continue* cn = CreateAST(Continue, tok_cn);
		return cn;
	}

	Import* parseImport(std::deque<Token*>& tokens)
	{
		assert(eat(tokens)->type == TType::Import);

		Token* tok_mod;
		if((tok_mod = eat(tokens))->type != TType::Identifier)
			error("Expected module name after 'import' statement.");

		return CreateAST(Import, tok_mod, tok_mod->text);
	}

	StringLiteral* parseStringLiteral(std::deque<Token*>& tokens)
	{
		assert(tokens.front()->type == TType::StringLiteral);
		Token* str = eat(tokens);

		return CreateAST(StringLiteral, str, str->text);
	}

	OpOverload* parseOpOverload(std::deque<Token*>& tokens)
	{
		if(!isParsingStruct)
			error("Can only overload operators in the context of a named aggregate type");

		assert(eat(tokens)->text == "operator");
		Token* op = eat(tokens);

		ArithmeticOp ao;
		switch(op->type)
		{
			case TType::Equal:
				ao = ArithmeticOp::Assign;
				break;

			case TType::EqualsTo:
				ao = ArithmeticOp::CmpEq;
				break;

			default:
				error("Unsupported operator overload on operator '%s'", op->text.c_str());
		}

		OpOverload* oo = CreateAST(OpOverload, op, ao);

		Token* fake = new Token();
		fake->posinfo = currentPos;
		fake->text = "operator#" + op->text;
		fake->type = TType::Identifier;

		tokens.push_front(fake);

		// parse a func declaration.
		oo->func = parseFunc(tokens);
		return oo;
	}
}

























