// parser_internal.h
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once

#include "defs.h"
#include "errors.h"

#include "ast.h"
#include "lexer.h"
#include "parser.h"

namespace pts
{
	struct Type;
}

namespace parser
{
	struct State
	{
		State(const lexer::TokenList& tl) : tokens(tl) { }

		const lexer::Token& lookahead(size_t num) const
		{
			if(this->index + num < this->tokens.size())
				return this->tokens[this->index + num];

			else
				error("lookahead %zu tokens > size %zu", num, this->tokens.size());
		}

		void skip(size_t num)
		{
			if(this->index + num < this->tokens.size())
				this->index += num;

			else
				error("skip %zu tokens > size %zu", num, this->tokens.size());
		}

		void rewind(size_t num)
		{
			if(this->index > num)
				this->index -= num;

			else
				error("rewind %zu tokens > index %zu", num, this->index);
		}

		void rewindTo(size_t ix)
		{
			if(ix >= this->tokens.size())
				error("ix %zu > size %zu", ix, this->tokens.size());

			this->index = ix;
		}

		const lexer::Token& pop()
		{
			if(this->index + 1 < this->tokens.size())
				return this->tokens[this->index++];

			else
				error("pop() on last token");
		}

		const lexer::Token& eat()
		{
			this->skipWS();
			if(this->index + 1 < this->tokens.size())
				return this->tokens[this->index++];

			else
				error("eat() on last token");
		}

		void skipWS()
		{
			while(this->tokens[this->index] == lexer::TokenType::NewLine || this->tokens[this->index] == lexer::TokenType::Comment)
				this->index++;
		}

		const lexer::Token& front() const
		{
			return this->tokens[this->index];
		}


		const lexer::Token& frontAfterWS()
		{
			size_t ofs = 0;
			while(this->tokens[this->index + ofs] == lexer::TokenType::NewLine
				|| this->tokens[this->index + ofs] == lexer::TokenType::Comment)
			{
				ofs++;
			}
			return this->tokens[this->index + ofs];
		}

		const lexer::Token& prev()
		{
			return this->tokens[this->index - 1];
		}

		const Location& loc() const
		{
			return this->front().loc;
		}

		const Location& ploc() const
		{
			iceAssert(this->index > 0);
			return this->tokens[this->index - 1].loc;
		}

		size_t remaining() const
		{
			return this->tokens.size() - this->index - 1;
		}

		size_t getIndex() const
		{
			return this->index;
		}

		void setIndex(size_t i)
		{
			iceAssert(i < tokens.size());
			this->index = i;
		}

		bool frontIsWS() const
		{
			return this->front() == lexer::TokenType::Comment || this->front() == lexer::TokenType::NewLine;
		}

		bool hasTokens() const
		{
			return this->index < this->tokens.size();
		}

		const lexer::TokenList& getTokenList()
		{
			return this->tokens;
		}


		// implicitly coerce to location, so we can
		// error(st, ...)
		operator const Location&() const
		{
			return this->loc();
		}

		std::string currentFilePath;

		std::unordered_map<std::string, parser::CustomOperatorDecl> binaryOps;
		std::unordered_map<std::string, parser::CustomOperatorDecl> prefixOps;
		std::unordered_map<std::string, parser::CustomOperatorDecl> postfixOps;

		private:
			size_t index = 0;
			const lexer::TokenList& tokens;
	};

	std::string parseOperatorTokens(State& st);

	pts::Type* parseType(State& st);
	ast::Expr* parseExpr(State& st);
	ast::Stmt* parseStmt(State& st);

	ast::DeferredStmt* parseDefer(State& st);

	ast::Stmt* parseVariable(State& st);
	ast::ReturnStmt* parseReturn(State& st);
	ast::ImportStmt* parseImport(State& st);
	ast::FuncDefn* parseFunction(State& st);
	ast::Stmt* parseStmtWithAccessSpec(State& st);
	ast::ForeignFuncDefn* parseForeignFunction(State& st);
	ast::OperatorOverloadDefn* parseOperatorOverload(State& st);

	std::map<size_t, std::tuple<std::string, bool, Location>> parseArrayDecomp(State& st);
	std::vector<ast::TupleDecompMapping> parseTupleDecomp(State& st);

	ast::EnumDefn* parseEnum(State& st);
	ast::ClassDefn* parseClass(State& st);
	ast::StructDefn* parseStruct(State& st);
	ast::StaticStmt* parseStaticStmt(State& st);

	ast::DeallocOp* parseDealloc(State& st);

	ast::Block* parseBracedBlock(State& st);

	ast::LitNumber* parseNumber(State& st);
	ast::LitString* parseString(State& st, bool israw);
	ast::LitArray* parseArray(State& st, bool israw);

	ast::Stmt* parseForLoop(State& st);
	ast::IfStmt* parseIfStmt(State& st);
	ast::WhileLoop* parseWhileLoop(State& st);

	ast::TopLevelBlock* parseTopLevel(State& st, std::string name);

	ast::Stmt* parseBreak(State& st);
	ast::Stmt* parseContinue(State& st);

	std::map<std::string, TypeConstraints_t> parseGenericTypeList(State& st);



}

























