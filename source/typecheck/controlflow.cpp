// controlflow.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"

TCResult ast::IfStmt::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	using Case = sst::IfStmt::Case;
	auto ret = new sst::IfStmt(this->loc);

	auto n = fs->getAnonymousScopeName();

	fs->pushTree(n);
	defer(fs->popTree());

	for(auto c : this->cases)
	{
		auto inits = util::map(c.inits, [fs](Stmt* s) -> auto { return s->typecheck(fs).stmt(); });
		auto cs = Case(c.cond->typecheck(fs).expr(), dynamic_cast<sst::Block*>(c.body->typecheck(fs).stmt()), inits);

		if(!cs.cond->type->isBoolType() && !cs.cond->type->isPointerType())
			error(cs.cond, "Non-boolean expression with type '%s' cannot be used as a conditional", cs.cond->type);

		ret->cases.push_back(cs);

		iceAssert(ret->cases.back().body);
	}

	if(this->elseCase)
	{
		ret->elseCase = dynamic_cast<sst::Block*>(this->elseCase->typecheck(fs).stmt());
		iceAssert(ret->elseCase);
	}

	return TCResult(ret);
}

TCResult ast::ReturnStmt::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	auto ret = new sst::ReturnStmt(this->loc);

	if(fs->isInDeferBlock())
		error(this, "Cannot 'return' while inside a deferred block");


	// ok, get the current function
	auto fn = fs->getCurrentFunction();
	auto retty = fn->returnType;

	if(this->value)
	{
		ret->value = this->value->typecheck(fs, retty).expr();

		if(ret->value->type != retty)
		{
			SpanError(SimpleError::make(this, "Mismatched type in return statement; function returns '%s', value has type '%s'", retty, ret->value->type))
				.add(SpanError::Span(this->value->loc, strprintf("type '%s'", ret->value->type)))
				.append(SimpleError(fn->loc, "Function definition is here:", MsgType::Note))
				.postAndQuit();
		}

		// ok
	}
	else if(!retty->isVoidType())
	{
		error(this, "Expected value after 'return'; function return type is '%s'", retty);
	}

	ret->expectedType = retty;
	return TCResult(ret);
}


static bool checkBlockPathsReturn(sst::TypecheckState* fs, sst::Block* block, fir::Type* retty, std::vector<sst::Block*>* faulty)
{
	// return value is whether or not the block had a return value;
	// true if all paths explicitly returned, false if not
	// this return value is used to determine whether we need to insert a
	// 'return void' thing.

	bool ret = false;
	for(size_t i = 0; i < block->statements.size(); i++)
	{
		auto& s = block->statements[i];
		if(auto hb = dcast(sst::HasBlocks, s))
		{
			const auto& blks = hb->getBlocks();
			for(auto b : blks)
			{
				auto r = checkBlockPathsReturn(fs, b, retty, faulty);
				if(!r) faulty->push_back(b);

				ret &= r;
			}
		}


		// check for returns
		else if(auto retstmt = dcast(sst::ReturnStmt, s))
		{
			// ok...
			ret = true;
			auto t = retstmt->expectedType;
			iceAssert(t);

			if(t != retty)
			{
				if(retstmt->expectedType->isVoidType())
				{
					error(retstmt, "Expected value after 'return'; function return type is '%s'", retty);
				}
				else
				{
					std::string msg;
					if(block->isSingleExpr) msg = "Invalid single-expression with type '%s' in function returning '%s'";
					else                    msg = "Mismatched type in return statement; function returns '%s', value has type '%s'";

					SpanError(SimpleError::make(retstmt, msg.c_str(), retty, retstmt->expectedType))
						.add(SpanError::Span(retstmt->value->loc, strprintf("type '%s'", retstmt->expectedType)))
						.append(SimpleError(fs->getCurrentFunction()->loc, "Function definition is here:", MsgType::Note))
						.postAndQuit();
				}
			}

			// ok, pass

			if(i != block->statements.size() - 1)
			{
				SimpleError::make(block->statements[i + 1], "Unreachable code after return statement")
					.append(SimpleError::make(MsgType::Note, retstmt, "Return statement was here:"))
					.postAndQuit();;

				doTheExit();
			}
		}
	}

	return ret;
}

bool sst::TypecheckState::checkAllPathsReturn(FunctionDefn* fn)
{
	fir::Type* expected = fn->returnType;

	std::vector<sst::Block*> faults { fn->body };
	auto ret = checkBlockPathsReturn(this, fn->body, expected, &faults);

	if(!expected->isVoidType() && !ret)
	{
		auto err = SimpleError::make(fn, "Not all paths return a value; expected value of type '%s'", expected);

		for(auto b : faults)
			err.append(SimpleError::make(MsgType::Note, b->closingBrace, "Potentially missing return statement here:"));

		err.postAndQuit();
	}

	return ret;
}


























