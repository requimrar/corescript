// LiteralCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"

using namespace Ast;
using namespace Codegen;

Result_t Number::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	// check builtin type
	if(this->decimal)
		return Result_t(fir::ConstantFP::getFloat64(this->dval), 0);

	else if(this->needUnsigned)
		return Result_t(fir::ConstantInt::getUint64((uint64_t) this->ival), 0);

	else
		return Result_t(fir::ConstantInt::getInt64(this->ival), 0);
}

fir::Type* Number::getType(CodegenInstance* cgi, bool allowFail, fir::Value* extra)
{
	if(this->decimal)
		return fir::PrimitiveType::getFloat64();

	else if(this->needUnsigned)
		return fir::PrimitiveType::getUint64();

	else
		return fir::PrimitiveType::getInt64();
}






Result_t BoolVal::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	return Result_t(fir::ConstantInt::getBool(this->val), 0);
}

fir::Type* BoolVal::getType(CodegenInstance* cgi, bool allowFail, fir::Value* extra)
{
	return fir::PrimitiveType::getBool();
}







Result_t NullVal::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	return Result_t(fir::ConstantValue::getNull(), 0);
}

fir::Type* NullVal::getType(CodegenInstance* cgi, bool allowFail, fir::Value* extra)
{
	return fir::PrimitiveType::getVoid()->getPointerTo();
}






Result_t StringLiteral::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	if(this->isRaw)
	{
		// good old Int8*
		fir::Value* stringVal = cgi->module->createGlobalString(this->str);
		stringVal = cgi->irb.CreateConstGEP2(stringVal, 0, 0);

		return Result_t(stringVal, 0);
	}
	else
	{
		if(extra)
		{
			iceAssert(extra->getType()->getPointerElementType()->isStringType());

			fir::Value* thestring = cgi->module->createGlobalString(this->str);
			thestring = cgi->irb.CreateConstGEP2(thestring, 0, 0);

			fir::Value* len = fir::ConstantInt::getInt32(this->str.length());
			fir::Value* rc = fir::ConstantInt::getInt32(-1);

			cgi->irb.CreateSetStringData(extra, thestring);
			cgi->irb.CreateSetStringLength(extra, len);
			cgi->irb.CreateSetStringRefCount(extra, rc);

			cgi->addRefCountedValue(extra);
			extra->setName("lit_<" + this->str + ">");
			return Result_t(cgi->irb.CreateLoad(extra), extra);
		}
		else
		{
			auto r = cgi->makeStringLiteral(this->str);
			r.pointer->setName("lit_<" + this->str + ">");

			return r;
		}
	}
}

fir::Type* StringLiteral::getType(CodegenInstance* cgi, bool allowFail, fir::Value* extra)
{
	if(this->isRaw)
		return fir::PointerType::getInt8Ptr();

	else
		return fir::StringType::get();
}












Result_t ArrayLiteral::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	fir::Type* tp = 0;
	std::vector<fir::ConstantValue*> vals;

	if(this->values.size() == 0)
	{
		if(!extra)
		{
			error(this, "Unable to infer type for empty array");
		}

		tp = extra->getType()->getPointerElementType();
	}
	else
	{
		tp = this->values.front()->getType(cgi);

		for(Expr* e : this->values)
		{
			fir::Value* v = e->codegen(cgi).value;
			if(dynamic_cast<fir::ConstantValue*>(v))
			{
				fir::ConstantValue* c = dynamic_cast<fir::ConstantValue*>(v);

				vals.push_back(c);
				if(vals.back()->getType() != tp)
				{
					error(e, "Array members must have the same type, got %s and %s",
						tp->str().c_str(), vals.back()->getType()->str().c_str());
				}
			}
			else
			{
				error(e, "Array literal members must be constant");
			}
		}
	}

	fir::ArrayType* atype = fir::ArrayType::get(tp, this->values.size());
	fir::Value* alloc = cgi->irb.CreateStackAlloc(atype);
	fir::Value* val = fir::ConstantArray::get(atype, vals);

	cgi->irb.CreateStore(val, alloc);
	return Result_t(val, alloc);
}

fir::Type* ArrayLiteral::getType(CodegenInstance* cgi, bool allowFail, fir::Value* extra)
{
	return fir::ArrayType::get(this->values.front()->getType(cgi), this->values.size());
}














