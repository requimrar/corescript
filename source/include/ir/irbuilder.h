// irbuilder.h
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <limits.h>


#include "errors.h"

#include <string>
#include <vector>
#include <deque>
#include <unordered_map>

#include "block.h"
#include "value.h"
#include "module.h"
#include "function.h"
#include "constant.h"

namespace fir
{
	struct IRBuilder
	{
		IRBuilder(FTContext* c);

		Value* CreateNeg(Value* a);
		Value* CreateAdd(Value* a, Value* b);
		Value* CreateSub(Value* a, Value* b);
		Value* CreateMul(Value* a, Value* b);
		Value* CreateDiv(Value* a, Value* b);
		Value* CreateMod(Value* a, Value* b);
		Value* CreateICmpEQ(Value* a, Value* b);
		Value* CreateICmpNEQ(Value* a, Value* b);
		Value* CreateICmpGT(Value* a, Value* b);
		Value* CreateICmpLT(Value* a, Value* b);
		Value* CreateICmpGEQ(Value* a, Value* b);
		Value* CreateICmpLEQ(Value* a, Value* b);
		Value* CreateFCmpEQ_ORD(Value* a, Value* b);
		Value* CreateFCmpEQ_UNORD(Value* a, Value* b);
		Value* CreateFCmpNEQ_ORD(Value* a, Value* b);
		Value* CreateFCmpNEQ_UNORD(Value* a, Value* b);

		Value* CreateFCmpGT_ORD(Value* a, Value* b);
		Value* CreateFCmpGT_UNORD(Value* a, Value* b);
		Value* CreateFCmpLT_ORD(Value* a, Value* b);
		Value* CreateFCmpLT_UNORD(Value* a, Value* b);
		Value* CreateFCmpGEQ_ORD(Value* a, Value* b);
		Value* CreateFCmpGEQ_UNORD(Value* a, Value* b);
		Value* CreateFCmpLEQ_ORD(Value* a, Value* b);
		Value* CreateFCmpLEQ_UNORD(Value* a, Value* b);

		Value* CreateLogicalAND(Value* a, Value* b);
		Value* CreateLogicalOR(Value* a, Value* b);
		Value* CreateBitwiseXOR(Value* a, Value* b);
		Value* CreateBitwiseLogicalSHR(Value* a, Value* b);
		Value* CreateBitwiseArithmeticSHR(Value* a, Value* b);
		Value* CreateBitwiseSHL(Value* a, Value* b);
		Value* CreateBitwiseAND(Value* a, Value* b);
		Value* CreateBitwiseOR(Value* a, Value* b);
		Value* CreateBitwiseNOT(Value* a);
		Value* CreateBitcast(Value* v, Type* targetType);
		Value* CreateIntSizeCast(Value* v, Type* targetType);
		Value* CreateFloatToIntCast(Value* v, Type* targetType);
		Value* CreateIntToFloatCast(Value* v, Type* targetType);
		Value* CreatePointerTypeCast(Value* v, Type* targetType);
		Value* CreatePointerToIntCast(Value* v, Type* targetType);
		Value* CreateIntToPointerCast(Value* v, Type* targetType);

		Value* CreateFTruncate(Value* v, Type* targetType);
		Value* CreateFExtend(Value* v, Type* targetType);

		Value* CreateLoad(Value* ptr);
		Value* CreateStore(Value* v, Value* ptr);
		Value* CreateCall0(Function* fn);
		Value* CreateCall1(Function* fn, Value* p1);
		Value* CreateCall2(Function* fn, Value* p1, Value* p2);
		Value* CreateCall3(Function* fn, Value* p1, Value* p2, Value* p3);
		Value* CreateCall(Function* fn, std::deque<Value*> args);
		Value* CreateCall(Function* fn, std::vector<Value*> args);
		Value* CreateCall(Function* fn, std::initializer_list<Value*> args);

		Value* CreateReturn(Value* v);
		Value* CreateReturnVoid();

		Value* CreateLogicalNot(Value* v);
		Value* CreateStackAlloc(Type* type);

		// equivalent to llvm's GEP(ptr*, ptrIndex, memberIndex)
		Value* CreateGetPointerToStructMember(Value* ptr, Value* ptrIndex, Value* memberIndex);
		Value* CreateGetPointerToConstStructMember(Value* ptr, Value* ptrIndex, size_t memberIndex);

		// equivalent to GEP(ptr*, 0, memberIndex)
		Value* CreateGetStructMember(Value* structPtr, Value* memberIndex);
		Value* CreateGetConstStructMember(Value* structPtr, size_t memberIndex);

		// equivalent to GEP(ptr*, index)
		Value* CreateGetPointer(Value* ptr, Value* ptrIndex);

		// equivalent to GEP(ptr*, ptrIndex, elmIndex)
		Value* CreateConstGEP2(Value* ptr, size_t ptrIndex, size_t elmIndex);

		void CreateCondBranch(Value* condition, IRBlock* trueBlock, IRBlock* falseBlock);
		void CreateUnCondBranch(IRBlock* target);

		IRBlock* addNewBlockInFunction(std::string name, Function* func);
		IRBlock* addNewBlockAfter(std::string name, IRBlock* block);


		void setCurrentBlock(IRBlock* block);
		void restorePreviousBlock();

		Function* getCurrentFunction();
		IRBlock* getCurrentBlock();

		Value* CreateBinaryOp(Ast::ArithmeticOp ao, Value* a, Value* b);

		private:
		Value* addInstruction(Instruction* instr);

		FTContext* context;

		Function* currentFunction = 0;
		IRBlock* currentBlock = 0;
		IRBlock* previousBlock = 0;
	};
}



















































