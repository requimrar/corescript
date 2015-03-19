// VarCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;


Result_t VarRef::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr, llvm::Value* rhs)
{
	llvm::Value* val = cgi->getSymInst(this, this->name);
	if(!val)
		GenError::unknownSymbol(this, this->name, SymbolType::Variable);

	printf("var ref: %s -> %s\n", this->name.c_str(), cgi->getReadableType(val->getType()->getPointerElementType()).c_str());
	return Result_t(cgi->mainBuilder.CreateLoad(val, this->name), val);
}

llvm::Value* VarDecl::doInitialValue(Codegen::CodegenInstance* cgi, TypePair_t* cmplxtype, llvm::Value* val, llvm::Value* valptr, llvm::Value* storage)
{
	llvm::Value* ai = storage;
	if(this->initVal && !cmplxtype && this->type != "Inferred")
	{
		// ...
	}
	else if(!this->initVal && (cgi->isBuiltinType(this) || cgi->isArrayType(this) || cgi->isPtr(this)))
	{
		val = cgi->getDefaultValue(this);
		assert(val);
	}
	else
	{
		if(this->inferredLType)
		{
			cmplxtype = cgi->getType(this->inferredLType);
		}
		else
		{
			if(this->type.find("::") != std::string::npos)
				cmplxtype = cgi->getType(cgi->mangleRawNamespace(this->type));

			else
				cmplxtype = cgi->getType(this->type);
		}

		if(!ai)
		{
			assert(cmplxtype);
			assert((ai = cgi->mainBuilder.CreateAlloca(cmplxtype->first)));
		}

		if(cmplxtype)
		{
			// automatically call the init() function
			if(!this->disableAutoInit && !this->initVal)
			{
				llvm::Value* oldAi = ai;
				ai = cgi->lastMinuteUnwrapType(oldAi);
				if(oldAi != ai)
				{
					cmplxtype = cgi->getType(ai->getType()->getPointerElementType());
					assert(cmplxtype);
				}

				std::vector<llvm::Value*> args { ai };

				llvm::Function* initfunc = cgi->getStructInitialiser(this, cmplxtype, args);
				val = cgi->mainBuilder.CreateCall(initfunc, args);
			}
		}

		cgi->addSymbol(this->name, ai, this);
		if(this->initVal && !cmplxtype)
		{
			// this only works if we don't call a constructor
			cgi->doBinOpAssign(this, new VarRef(this->posinfo, this->name), this->initVal, ArithmeticOp::Assign, val, ai, val);
			return val;
		}
		else if(!cmplxtype && !this->initVal)
		{
			if(!val)
				val = cgi->getDefaultValue(this);

			cgi->mainBuilder.CreateStore(val, ai);
			return val;
		}
		else if(cmplxtype && this->initVal)
		{
			ai = cgi->lastMinuteUnwrapType(ai);

			cgi->mainBuilder.CreateStore(val, ai);
			return val;
		}
		else
		{
			if(valptr)
				val = cgi->mainBuilder.CreateLoad(valptr);

			else
				return val;
		}
	}

	if(!ai)
		error(this, "ai is null");

	printf("var %s has type %s\n", this->name.c_str(), cgi->getReadableType(ai->getType()->getPointerElementType()).c_str());
	if(!cgi->isDuplicateSymbol(this->name))
		cgi->addSymbol(this->name, ai, this);

	if(val->getType() != ai->getType()->getPointerElementType())
	{
		Number* n = 0;
		if(val->getType()->isIntegerTy() && (n = dynamic_cast<Number*>(this->initVal)) && n->ival == 0)
		{
			val = llvm::Constant::getNullValue(ai->getType()->getPointerElementType());
		}
		else if(val->getType()->isIntegerTy() && ai->getType()->getPointerElementType()->isIntegerTy())
		{
			Number* n = 0;
			if((n = dynamic_cast<Number*>(this->initVal)))
			{
				uint64_t max = pow(2, val->getType()->getIntegerBitWidth());
				if(max == 0) max = UINT64_MAX;
				if((uint64_t) n->ival < max)
					val = cgi->mainBuilder.CreateIntCast(val, ai->getType()->getPointerElementType(), false);
			}
		}
		else
		{
			GenError::invalidAssignment(this, val->getType(), ai->getType()->getPointerElementType());
		}
	}

	cgi->mainBuilder.CreateStore(val, ai);
	return val;
}

Result_t VarDecl::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr, llvm::Value* rhs)
{
	if(cgi->isDuplicateSymbol(this->name))
		GenError::duplicateSymbol(this, this->name, SymbolType::Variable);

	llvm::Value* val = nullptr;
	llvm::Value* valptr = nullptr;

	TypePair_t* cmplxtype = cgi->getType(this->type);
	if(!cmplxtype) cmplxtype = cgi->getType(cgi->mangleRawNamespace(this->type));

	llvm::Value* ai = nullptr;

	if(this->type == "Inferred")
	{
		if(!this->initVal)
			error(this, "Type inference requires an initial assignment to infer type");

		assert(!cmplxtype);

		if(!this->isGlobal)
		{
			ai = cgi->allocateInstanceInBlock(cgi->getLlvmType(this->initVal), this->name);
		}

		auto r = this->initVal->codegen(cgi, ai).result;

		val = r.first;
		valptr = r.second;
		this->inferredLType = cgi->getLlvmType(this->initVal);

		if(cgi->isBuiltinType(this->initVal) && !this->inferredLType->isStructTy())
			this->type = cgi->getReadableType(this->initVal);
	}
	else if(this->initVal)
	{
		if(!this->isGlobal)
		{
			ai = cgi->allocateInstanceInBlock(this);
		}

		auto r = this->initVal->codegen(cgi, ai).result;

		val = r.first;
		valptr = r.second;

		this->inferredLType = cgi->getLlvmType(this);
	}
	else
	{
		if(!this->isGlobal)
		{
			ai = cgi->allocateInstanceInBlock(this);
		}
		this->inferredLType = cgi->getLlvmType(this);
	}

	// TODO: call global constructors
	if(this->isGlobal)
	{
		ai = new llvm::GlobalVariable(*cgi->mainModule, this->inferredLType, this->immutable,
			this->attribs & Attr_VisPublic ? llvm::GlobalValue::ExternalLinkage : llvm::GlobalValue::InternalLinkage, llvm::Constant::getNullValue(this->inferredLType), this->name);

		if(this->initVal)
		{
			assert(val);

			if(llvm::cast<llvm::Constant>(val))
			{
				cgi->autoCastType(ai->getType()->getPointerElementType(), val, valptr);
				printf("GLOBAL: (%s) -> (%s, %s)\n", this->name.c_str(), cgi->getReadableType(ai->getType()->getPointerElementType()).c_str(), cgi->getReadableType(val).c_str());
				llvm::cast<llvm::GlobalVariable>(ai)->setInitializer(llvm::cast<llvm::Constant>(val));
			}
			else
			{
				warn(this, "Global variables currently only support constant initialisers");
			}
		}


		cgi->addSymbol(this->name, ai, this);
	}
	else
	{
		this->doInitialValue(cgi, cmplxtype, val, valptr, ai);
	}

	return Result_t(val, ai);
}














