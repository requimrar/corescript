// AllocCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/ast.h"
#include "../include/codegen.h"

#include "llvm/IR/Module.h"

using namespace Ast;
using namespace Codegen;


#define MALLOC_FUNC		"malloc"
#define FREE_FUNC		"free"
#define MEMSET_FUNC		"memset"


Result_t Alloc::codegen(CodegenInstance* cgi, fir::Value* lhsPtr, fir::Value* rhs)
{
	// if we haven't declared malloc() yet, then we need to do it here
	// NOTE: this is the only place in the compiler where a hardcoded call is made to a non-provided function.

	FuncPair_t* fp = cgi->getOrDeclareLibCFunc(MALLOC_FUNC);

	fir::Function* mallocf = fp->first;
	iceAssert(mallocf);

	mallocf = cgi->module->getFunction(mallocf->getName());
	iceAssert(mallocf);

	fir::Type* allocType = 0;

	allocType = cgi->getLlvmTypeFromExprType(this, this->type);
	iceAssert(allocType);





	// call malloc
	// todo: all broken

	#if 0

	// fir::Value* oneValue = fir::ConstantInt::getConstantUIntValue(fir::PrimitiveType::getInt64(cgi->getContext()), 1);
	// fir::Value* zeroValue = fir::ConstantInt::getConstantUIntValue(fir::PrimitiveType::getInt64(cgi->getContext()), 0);

	uint64_t typesize = cgi->module->getDataLayout()->getTypeSizeInBits(allocType) / 8;
	fir::Value* allocsize = fir::ConstantInt::get(fir::IntegerType::getInt64Ty(cgi->getContext()), typesize);
	fir::Value* allocnum = oneValue;


	fir::Value* isZero = nullptr;
	if(this->count)
	{
		allocnum = this->count->codegen(cgi).result.first;
		if(!allocnum->getType()->isIntegerTy())
			error(this, "Expected integer type in alloc");

		allocnum = cgi->builder.CreateIntCast(allocnum, allocsize->getType(), false);
		allocsize = cgi->builder.CreateMul(allocsize, allocnum);


		// compare to zero. first see what we can do at compile time, if the constant is zero.
		if(fir::isa<fir::ConstantInt>(allocnum))
		{
			fir::ConstantInt* ci = fir::cast<fir::ConstantInt>(allocnum);
			if(ci->isZeroValue())
			{
				warn(this, "Allocating zero members with alloc[], will return null");
				isZero = fir::ConstantInt::getTrue(cgi->getContext());
			}
		}
		else
		{
			// do it at runtime.
			isZero = cgi->builder.CreateICmpEQ(allocnum, fir::ConstantInt::getNullValue(allocsize->getType()));
		}
	}

	fir::Value* allocmemptr = lhsPtr ? lhsPtr : cgi->allocateInstanceInBlock(allocType->getPointerTo());

	fir::Value* amem = cgi->builder.CreatePointerCast(cgi->builder.CreateCall(mallocf, allocsize), allocType->getPointerTo());
	// warn(cgi, this, "%s -> %s\n", cgi->getReadableType(amem).c_str(), cgi->getReadableType(allocmemptr).c_str());

	cgi->builder.CreateStore(amem, allocmemptr);
	fir::Value* allocatedmem = cgi->builder.CreateLoad(allocmemptr);

	// call the initialiser, if there is one
	if(allocType->isIntegerTy() || allocType->isPointerTy())
	{
		fir::Value* cs = cgi->builder.CreatePointerBitCastOrAddrSpaceCast(allocatedmem, fir::PointerType::getInt8PtrTy(cgi->getContext()));
		fir::Value* dval = fir::Constant::getNullValue(cs->getType()->getPointerElementType());

		// printf("%s, %s, %s, %llu\n", cgi->getReadableType(cs).c_str(), cgi->getReadableType(dval).c_str(),
			// cgi->getReadableType(allocsize).c_str(), typesize);

		cgi->builder.CreateMemSet(cs, dval, allocsize, typesize);
	}
	else
	{
		TypePair_t* typePair = 0;

		std::vector<fir::Value*> args;
		args.push_back(allocatedmem);
		for(Expr* e : this->params)
			args.push_back(e->codegen(cgi).result.first);

		typePair = cgi->getType(allocType);

		fir::Function* initfunc = cgi->getStructInitialiser(this, typePair, args);

		// we need to keep calling this... essentially looping.
		fir::BasicBlock* curbb = cgi->builder.GetInsertBlock();	// store the current bb
		fir::BasicBlock* loopBegin = fir::BasicBlock::Create(cgi->getContext(), "loopBegin", curbb->getParent());
		fir::BasicBlock* loopEnd = fir::BasicBlock::Create(cgi->getContext(), "loopEnd", curbb->getParent());
		fir::BasicBlock* after = fir::BasicBlock::Create(cgi->getContext(), "afterLoop", curbb->getParent());



		// check for zero.
		if(isZero)
		{
			fir::BasicBlock* notZero = fir::BasicBlock::Create(cgi->getContext(), "notZero", curbb->getParent());
			fir::BasicBlock* setToZero = fir::BasicBlock::Create(cgi->getContext(), "zeroAlloc", curbb->getParent());
			cgi->builder.CreateCondBr(isZero, setToZero, notZero);

			cgi->builder.SetInsertPoint(setToZero);
			cgi->builder.CreateStore(fir::ConstantPointerNull::getNullValue(allocatedmem->getType()), allocmemptr);
			allocatedmem = cgi->builder.CreateLoad(allocmemptr);
			cgi->builder.CreateBr(after);

			cgi->builder.SetInsertPoint(notZero);
		}









		// create the loop counter (initialise it with the value)
		fir::Value* counterptr = cgi->allocateInstanceInBlock(allocsize->getType());
		cgi->builder.CreateStore(allocnum, counterptr);

		// do { ...; num--; } while(num - 1 > 0)
		cgi->builder.CreateBr(loopBegin);	// explicit branch


		// start in the loop
		cgi->builder.SetInsertPoint(loopBegin);

		// call the constructor
		allocatedmem = cgi->builder.CreateLoad(allocmemptr);
		args[0] = allocatedmem;
		cgi->builder.CreateCall(initfunc, args);

		// move the allocatedmem pointer by the type size
		cgi->doPointerArithmetic(ArithmeticOp::Add, allocatedmem, allocmemptr, oneValue);
		allocatedmem = cgi->builder.CreateLoad(allocmemptr);

		// subtract the counter
		fir::Value* counter = cgi->builder.CreateLoad(counterptr);
		cgi->builder.CreateStore(cgi->builder.CreateSub(counter, oneValue), counterptr);

		// do the comparison
		counter = cgi->builder.CreateLoad(counterptr);

		fir::Value* brcond = cgi->builder.CreateICmpUGT(counter, zeroValue);
		cgi->builder.CreateCondBr(brcond, loopBegin, loopEnd);

		// at loopend:
		cgi->builder.SetInsertPoint(loopEnd);

		// undo the pointer additions we did above
		cgi->doPointerArithmetic(ArithmeticOp::Subtract, allocatedmem, allocmemptr, allocnum);

		allocatedmem = cgi->builder.CreateLoad(allocmemptr);

		cgi->doPointerArithmetic(ArithmeticOp::Add, allocatedmem, allocmemptr, oneValue);


		cgi->builder.CreateBr(after);
		cgi->builder.SetInsertPoint(after);
		allocatedmem = cgi->builder.CreateLoad(allocmemptr);
	}

	return Result_t(allocatedmem, 0);
	#endif

	return Result_t(0, 0);
}


Result_t Dealloc::codegen(CodegenInstance* cgi, fir::Value* lhsPtr, fir::Value* rhs)
{
	fir::Value* freearg = 0;
	if(dynamic_cast<VarRef*>(this->expr))
	{
		SymbolPair_t* sp = cgi->getSymPair(this, dynamic_cast<VarRef*>(this->expr)->name);
		if(!sp)
			error(this, "Unknown symbol '%s'", dynamic_cast<VarRef*>(this->expr)->name.c_str());


		// this will be an alloca instance (aka pointer to whatever type it actually was)
		fir::Value* varval = sp->first;

		// therefore, create a Load to get the actual value
		varval = cgi->builder.CreateLoad(varval);
		freearg = cgi->builder.CreatePointerTypeCast(varval, fir::PointerType::getInt8Ptr(cgi->getContext()));
	}
	else
	{
		freearg = this->expr->codegen(cgi).result.first;
	}

	// call 'free'
	FuncPair_t* fp = cgi->getOrDeclareLibCFunc(FREE_FUNC);


	fir::Function* freef = fp->first;
	iceAssert(freef);

	freef = cgi->module->getFunction(freef->getName());
	iceAssert(freef);


	cgi->builder.CreateCall1(freef, freearg);
	return Result_t(0, 0);
}





















