// assign.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"

#define dcast(t, v)		dynamic_cast<t*>(v)

sst::AssignOp::AssignOp(const Location& l) : Expr(l, fir::Type::getVoid()) { }

CGResult sst::AssignOp::_codegen(cgn::CodegenState* cs, fir::Type* infer)
{
	auto lr = this->left->codegen(cs);
	auto lt = lr.value->getType();

	if(!lr.pointer || lr.kind != CGResult::VK::LValue)
	{
		HighlightOptions hs;
		hs.underlines.push_back(this->left->loc);
		error(this, hs, "Cannot assign to non-lvalue (most likely a temporary) expression");
	}

	if(lr.value->isImmutable() || (lr.pointer && lr.pointer->isImmutable()))
	{
		HighlightOptions hs;
		hs.underlines.push_back(this->left->loc);
		error(this, hs, "Cannot assign to immutable expression");
	}


	// check if we're trying to modify a literal, first of all.
	// we do it here, because we need some special sauce to do stuff
	if(auto so = dcast(SubscriptOp, this->left); so && so->cgSubscriptee->getType()->isStringType())
	{
		// yes, yes we are.
		auto checkf = cgn::glue::string::getCheckLiteralWriteFunction(cs);
		auto locstr = fir::ConstantString::get(this->loc.toString());

		// call it
		cs->irb.Call(checkf, so->cgSubscriptee, so->cgIndex, locstr);
	}


	// okay, i guess
	auto rr = this->right->codegen(cs, lt);
	auto rt = rr.value->getType();

	if(this->op != Operator::Assign)
	{
		// ok it's a compound assignment
		// auto [ newl, newr ] = cs->autoCastValueTypes(lr, rr);
		Operator nonass = getNonAssignOp(this->op);

		// some things -- if we're doing +=, and the types are supported, then just call the actual
		// append function, instead of doing the + first then assigning it.

		if(nonass == Operator::Add)
		{
			if(lt->isDynamicArrayType() && lt == rt)
			{
				// right then.
				if(lr.kind != CGResult::VK::LValue)
					error(this, "Cannot append to an r-value array");

				iceAssert(lr.pointer);
				auto appendf = cgn::glue::array::getAppendFunction(cs, lt->toDynamicArrayType());

				//? are there any ramifications for these actions for ref-counted things?
				auto res = cs->irb.Call(appendf, lr.value, rr.value);

				cs->irb.Store(res, lr.pointer);
				return CGResult(0);
			}
			else if(lt->isDynamicArrayType() && lt->getArrayElementType() == rt)
			{
				// right then.
				if(lr.kind != CGResult::VK::LValue)
					error(this, "Cannot append to an r-value array");

				iceAssert(lr.pointer);
				auto appendf = cgn::glue::array::getElementAppendFunction(cs, lt->toDynamicArrayType());

				//? are there any ramifications for these actions for ref-counted things?
				auto res = cs->irb.Call(appendf, lr.value, rr.value);

				cs->irb.Store(res, lr.pointer);
				return CGResult(0);
			}
		}


		// do the op first
		auto res = cs->performBinaryOperation(this->loc, { this->left->loc, lr }, { this->right->loc, rr }, nonass);

		// assign the res to the thing
		rr = res;
	}

	rr = cs->oneWayAutocast(rr, lt);

	if(rr.value == 0)
	{
		error(this, "Invalid assignment from value of type '%s' to expected type '%s'", rr.value->getType(),
			lt);
	}

	// ok then
	if(lt != rr.value->getType())
		error(this, "What? left = %s, right = %s", lt, rr.value->getType());

	iceAssert(lr.pointer);
	iceAssert(rr.value->getType() == lr.pointer->getType()->getPointerElementType());


	if(cs->isRefCountedType(lt))
	{
		if(rr.kind == CGResult::VK::LValue)
			cs->performRefCountingAssignment(lr, rr, false);

		else
			cs->moveRefCountedValue(lr, rr, false);
	}
	else
	{
		cs->irb.Store(rr.value, lr.pointer);
	}

	return CGResult(0);
}














