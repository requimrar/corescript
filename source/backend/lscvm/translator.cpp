// lscvm/translator.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#include <fstream>

#include "ir/module.h"

#include "frontend.h"
#include "backends/lscvm.h"


const std::string CONST_0               = "a";
const std::string CONST_1               = "b";
const std::string CONST_2               = "c";
const std::string CONST_3               = "d";
const std::string CONST_4               = "e";
const std::string CONST_5               = "f";
const std::string CONST_6               = "g";
const std::string CONST_7               = "h";
const std::string CONST_8               = "i";
const std::string CONST_9               = "j";

const std::string OP_ADD                = "A";
const std::string OP_HALT               = "B";
const std::string OP_CALL               = "C";
const std::string OP_DROP               = "D";
const std::string OP_READ_MEM           = "E";
const std::string OP_FETCH_STACK        = "F";
const std::string OP_JMP_REL            = "G";
const std::string OP_YANK_STACK         = "H";
const std::string OP_PRINT_INT          = "I";
const std::string OP_COMPARE            = "J";
const std::string OP_WRITE_MEM          = "K";
const std::string OP_MULTIPLY           = "M";
const std::string OP_PRINT_CHAR         = "P";
const std::string OP_RETURN             = "R";
const std::string OP_SUBTRACT           = "S";
const std::string OP_DIVIDE             = "V";
const std::string OP_JMP_REL_IF_ZERO    = "Z";

const std::string INTRINSIC_HALT        = "lscvm.B";
const std::string INTRINSIC_PRINT_CHAR  = "lscvm.P";
const std::string INTRINSIC_PRINT_INT   = "lscvm.I";
const std::string INTRINSIC_DUMP_STACK  = "lscvm.?";
const std::string INTRINSIC_DUMP_MEMORY = "lscvm.!";


constexpr size_t SLICE_DATA_INDEX       = 0;
constexpr size_t SLICE_LENGTH_INDEX     = 1;

constexpr size_t SAA_DATA_INDEX         = 0;
constexpr size_t SAA_LENGTH_INDEX       = 1;
constexpr size_t SAA_CAPACITY_INDEX     = 2;
constexpr size_t SAA_REFCOUNTPTR_INDEX  = 3;

constexpr size_t ANY_TYPEID_INDEX       = 0;
constexpr size_t ANY_REFCOUNTPTR_INDEX  = 1;
constexpr size_t ANY_DATA_ARRAY_INDEX   = 2;



std::string createNumber(int num)
{
	if(num < 0)
	{
		return "a" + createNumber(-num) + "S";
	}
	else if(num < 10)
	{
		return std::string(1, (char) (num + 'a'));
	}
	else if(num == 10)
	{
		return "cfM";
	}
	else
	{
		switch(num)
		{
			case 'a': return "jjMjAhA";
			case 'b': return "jjMjAiA";
			case 'c': return "jjMjAjA";
			case 'd': return "cfMcfMM";
			case 'e': return "jggAMhS";
			case 'f': return "jiAcdMM";
			case 'g': return "jggAMfS";
			case 'h': return "jeAiM";
			case 'i': return "hdfMM";
			case 'j': return "hdfMMbA";
			case 'A': return "iiMbA";
			case 'B': return "iiMcA";
			case 'C': return "iiMdA";
			case 'D': return "iiMeA";
			case 'E': return "iiMfA";
			case 'F': return "iiMgA";
			case 'G': return "iiMhA";
			case 'H': return "iiMiA";
			case 'I': return "iiMjA";
			case 'J': return "ijMcA";
			case 'K': return "ijMdA";
			case 'M': return "ijMfA";
			case 'P': return "ijMiA";
			case 'R': return "jjMbA";
			case 'S': return "jjMcA";
			case 'V': return "jjMfA";
			case 'Z': return "jjMjA";
			case '!': return "fgAdM";
			case '-': return "fddMM";

			// if we run into code size problems, i'm sure this algo can be optimised.
			default: {
				std::string ret;

				int x = num / 9;
				ret = createNumber(x) + CONST_9 + "M";

				int y = num % 9;
				if(y > 0) ret += createNumber(y) + "A";

				return ret;
			} break;
		}
	}
}


namespace backend
{
	struct State
	{
		fir::Module* firmod = 0;

		std::string program;

		int32_t relocationOffset = 0;

		util::hash_map<size_t, int32_t> memoryValueMap;

		// this must run first to set up all our constants.
		std::vector<std::string> memoryInitialisers;

		// so we can jump around.
		util::hash_map<size_t, int32_t> functionLocations;
		util::hash_map<size_t, int32_t> basicBlockLocations;

		// map from the instruction index (ie. index in program) to the fir ID of the target block
		// we need to replace the instruction (or value) at that location with the real address...
		util::hash_map<int32_t, size_t> relocations;

		// same as relocations, but we'll replace them with relative values. pair is { id, current_pc }.
		util::hash_map<int32_t, std::pair<size_t, int32_t>> relativeRelocations;

		// watermark for constant memory -- starts at CONSTANT_OFFSET_IN_MEMORY (0x12000)
		int32_t constantMemoryWatermark = 0;

		util::hash_map<fir::ConstantValue*, std::string> cachedConstants;

		int32_t currentStackFrameSize = 0;
		util::hash_map<size_t, int32_t> stackFrameValueMap;


		size_t currentStackOffset = 0;

		// usually this is the same as `currentStackOffset`, except when there are arguments;
		// then, numberOfLocalValues < currentStackOffset. the latter tracks the number of values on the stack, the former
		// tracks how many locals we need to pop on function epilogue --- this is necessary cos we use cdecl convention, where
		// the caller cleans the stack.
		size_t numberOfLocalValues = 0;

		// the amount of space taken by operands for each opcode that we are translating. once the opcode is done being
		// translated, there's supposed to be no extra stuff left on the stack, except any output values. the whole reason
		// we need this, is because the act of pushing an operand onto the stack by its nature needs to modify currentStackOffset,
		// because subsequent opcodes need a larger index into the stack to fetch their values.
		size_t temporaryStackValueOffset = 0;

		util::hash_map<size_t, int32_t> stackValues;

		util::hash_map<std::string, int32_t> stringTable;
	};

	constexpr size_t WORD_SIZE                      = 4;

	constexpr int32_t MAX_RELOCATION_SIZE           = 24;

	// limits are imposed by the vm!
	constexpr int32_t MAX_PROGRAM_SIZE              = 0x2000;

	constexpr int32_t STACK_POINTER_IN_MEMORY       = 0x00000;
	constexpr int32_t STACK_FRAME_IN_MEMORY         = 0x00008;

	constexpr int32_t CONSTANT_OFFSET_IN_MEMORY     = 0x00F0;
	constexpr int32_t MAX_MEMORY_SIZE               = 0x13880;

	// spaces are also no-ops, so that's good.
	const std::string EMPTY_RELOCATION              = std::string(MAX_RELOCATION_SIZE, ' ');


	/*
		! convention !
		* multi-word values are stored in BIG-ENDIAN FORMAT!!!


		* function calling
		arguments are pushed RIGHT TO LEFT. ie. the last argument will be pushed first
		this follows cdecl calling convention.

		since we have no registers, return value will be pushed on the stack before a return. in effect,
		doing 'C' will pop the function and any arguments, the push the return value (if any).

		typechecking should have ensured we don't try to do anything funny with void functions

		so before a call, the stack will look like this, for some foo(1, 2, 3)
		[ 3, 2, 1, <foo> ].

		there are no registers so there's nothing to preserve.


		* local variables
		since we're doing SSA, everything is immutable. we can use 'F' to fetch from the stack, so all those
		temporary values can just live on the stack.

		for allocas, we must spill them to memory, because we can't modify the contents of the stack.

		note that the lscvm backend has an additional restriction which might fuck us --- temporary locals (ie. non-lvalues) cannot
		be shared across blocks. non-lvalue locals (SSA temporaries) are dropped at the end of every block to prevent stack overflow,
		because we can't directly modify the "Real Stack" pointer (to save/restore and drop everything at once).

		so far it's not an issue, but we might get fucked later if programs get a bit more serious (PHI NODES!)
	*/















	static size_t getSizeInWords(fir::Type* ty)
	{
		auto sz = fir::getSizeOfType(ty);
		if(sz == 0) return 0;

		return std::max((size_t) 1, sz / WORD_SIZE);
	}




	static std::pair<bool, std::string> peephole(const std::string& a, const std::string& b)
	{
		if(a == CONST_0 && (b == OP_YANK_STACK || b == OP_ADD || b == OP_SUBTRACT))
			return { true, "" };

		if(a == CONST_1 && (b == OP_MULTIPLY || b == OP_DIVIDE))
			return { true, "" };

		return { false, "" };
	}

	static std::string makeinstr(const std::vector<std::string>& instrs)
	{
		std::stringstream ss;

		for(size_t i = 0; i < instrs.size(); i++)
		{
			if(i + 1 < instrs.size())
			{
				auto [ success, replacement ] = peephole(instrs[i], instrs[i+1]);
				if(success) (ss << replacement), i++;
				else        ss << instrs[i];
			}
			else
			{
				ss << instrs[i];
			}
		}

		return ss.str();
	}

	static std::string makeinstr(const std::string& s)
	{
		return s;
	}



	static int32_t createGlobalString(State* st, const std::string& str, int32_t* _ofs = 0)
	{
		if(auto it = st->stringTable.find(str); it != st->stringTable.end())
			return it->second;

		auto ret = st->constantMemoryWatermark;

		if(!_ofs)
			st->memoryInitialisers.push_back(createNumber(st->constantMemoryWatermark));

		std::string init;

		int32_t ofs = (_ofs ? *_ofs : 0);
		int32_t origofs = ofs;

		for(char c : str)
		{
			init += makeinstr({ createNumber(c), CONST_1, OP_FETCH_STACK, createNumber(ofs), OP_ADD, OP_WRITE_MEM });
			ofs++;
		}

		// null terminate!
		init += makeinstr({ CONST_0, createNumber(ofs), OP_WRITE_MEM });
		ofs++;

		st->memoryInitialisers.push_back(init);
		st->constantMemoryWatermark += (ofs - origofs);

		// then drop it.
		if(!_ofs)
			st->memoryInitialisers.push_back(makeinstr(OP_DROP));

		else
			*_ofs = ofs;

		st->stringTable[str] = ret;
		return ret;
	}






	static std::string createConstant(State* st, fir::ConstantValue* c)
	{
		if(auto it = st->cachedConstants.find(c); it != st->cachedConstants.end())
			return it->second;


		if(auto cint = dcast(fir::ConstantInt, c))
		{
			std::string ret = "";
			if(cint->getType()->toPrimitiveType()->isSigned())
				ret = createNumber(cint->getSignedValue());

			else
				ret = createNumber(cint->getUnsignedValue());

			// we don't support integers > 32-bits, but just fill in the rest with 0s.
			for(size_t i = 1; i < getSizeInWords(c->getType()); i++)
				ret += makeinstr(CONST_0);

			st->cachedConstants[c] = ret;
			return ret;
		}
		else if(auto cslice = dcast(fir::ConstantArraySlice, c))
		{
			auto ptr = createConstant(st, cslice->getData());
			auto len = createConstant(st, cslice->getLength());

			// just push the pointer followed by the length.
			auto ret = (ptr + len);

			st->cachedConstants[c] = ret;
			return ret;
		}
		else if(auto cstr = dcast(fir::ConstantString, c))
		{
			auto ofs = createGlobalString(st, cstr->getValue());

			// this is really just a char slice, so make the pointer:
			auto ptr = createNumber(ofs);
			auto len = createNumber(cstr->getValue().size());

			auto ret = (ptr + len);

			st->cachedConstants[c] = ret;
			return ret;
		}
		else if(auto cglobal = dcast(fir::GlobalVariable, c))
		{
			if(auto it = st->memoryValueMap.find(cglobal->id); it != st->memoryValueMap.end())
				return createNumber(it->second);

			else
				error("global (id %zu) has not been created yet", cglobal->id);
		}
		else
		{
			warn("unsupported constant (id %zu) (%s)", c->id, c->getType());
			return "";
		}
	}


	static std::string calcAddrInStackFrame(State* st, int32_t addr)
	{
		// basically, read from the current stack pointer,
		// subtract the maxstackwatermark, add the address.
		auto ofs = st->currentStackFrameSize - addr;
		return makeinstr({ createNumber(STACK_POINTER_IN_MEMORY), OP_READ_MEM, createNumber(ofs), OP_SUBTRACT });
	};


	static void fetchValue(State* st, fir::Value* fv)
	{
		if(auto fn = dcast(fir::Function, fv))
		{
			// hmm.
		}
		else if(auto bb = dcast(fir::IRBlock, fv))
		{
			error("don't use getValue with basic block");
		}
		else if(auto cv = dcast(fir::ConstantValue, fv))
		{
			st->program += createConstant(st, cv);
			st->temporaryStackValueOffset += getSizeInWords(fv->getType());
		}
		else
		{
			if(auto it = st->stackFrameValueMap.find(fv->id); it != st->stackFrameValueMap.end())
			{
				st->program += calcAddrInStackFrame(st, it->second);
				st->temporaryStackValueOffset++;
			}
			else if(auto it = st->stackValues.find(fv->id); it != st->stackValues.end())
			{
				auto sz = getSizeInWords(fv->getType());

				// we need to fetch the number from deep in the stack, possibly.
				// calculate how far back we need to go. 0 = it's at the top already, up to a max of
				// st->currentStackOffset - 1.

				std::string ret;

				// fetch however many words it needs.
				for(size_t i = 0; i < sz; i++)
				{
					auto ofs = createNumber(st->currentStackOffset + st->temporaryStackValueOffset + i - 1 - it->second);

					// fetch it.
					ret += makeinstr({ ofs, OP_FETCH_STACK });
				}

				st->program += ret;
				st->temporaryStackValueOffset += sz;
			}
			else
			{
				error("no value for id '%zu'", fv->id);
			}
		}
	}

	static void recordLocalOnStack(State* st, fir::Value* v, size_t ofs = -1)
	{
		if(ofs == -1) ofs = st->currentStackOffset;

		auto sz = getSizeInWords(v->getType());

		st->stackValues[v->id] = (ofs - (sz - 1));
		st->currentStackOffset += sz;
		st->numberOfLocalValues += sz;
	}

	static void addRelocation(State* st, fir::Value* val, int32_t location = -1)
	{
		st->relocations[location == -1 ? st->program.size() : location] = val->id;
		st->program += makeinstr(EMPTY_RELOCATION);
	}

	// if you don't provide 'pc', then it assumes the relative jump instruction (G or Z) immediately follows this constant!
	static void addRelativeRelocation(State* st, fir::Value* val, int32_t pc = -1, int32_t location = -1)
	{
		st->relativeRelocations[location == -1 ? st->program.size() : location] = {
			val->id, (int32_t) (pc == -1 ? (st->program.size() + MAX_RELOCATION_SIZE + 1) : pc)
		};
		st->program += makeinstr(EMPTY_RELOCATION);
	}



	void LSCVMBackend::performCompilation()
	{
		State _st;
		auto st = &_st;

		st->firmod = this->compiledData.module;

		st->constantMemoryWatermark = CONSTANT_OFFSET_IN_MEMORY;

		if(st->firmod->_getGlobalStrings().size() > 0)
		{
			st->memoryInitialisers.push_back(createNumber(st->constantMemoryWatermark));

			int32_t ofs = 0;
			for(auto str : st->firmod->_getGlobalStrings())
			{
				auto pos = createGlobalString(st, str.first, &ofs);
				st->memoryValueMap[str.second->id] = pos;
			}

			// then drop it.
			st->memoryInitialisers.push_back(makeinstr(OP_DROP));
		}



		// setup the stack pointer.
		{
			auto sp_addr = createNumber(STACK_POINTER_IN_MEMORY);   // 0x10000
			auto sp = createNumber(STACK_FRAME_IN_MEMORY);          // 0x10004

			st->memoryInitialisers.push_back(makeinstr({ sp, sp_addr, OP_WRITE_MEM }));
		}

		// add a jump to the global init function.
		// addRelocation(st, st->firmod->getFunction(Identifier("__global_init_function__", IdKind::Name)));
		// st->program += makeinstr({ OP_CALL });

		// then, call main:
		addRelocation(st, st->firmod->getEntryFunction());
		st->program += makeinstr(OP_CALL);

		// then, quit.
		st->program += makeinstr(OP_HALT);







		auto decay = [&st](fir::Value* fv) {
			fetchValue(st, fv);

			if(fv->islorclvalue())
			{
				// we need to fetch each word to the stack.
				size_t sz = getSizeInWords(fv->getType());

				// the address is already on top of the stack -- we just use fetch to get it again.
				for(size_t i = 0; i < sz; i++)
				{
					st->program += makeinstr({ createNumber(i), OP_FETCH_STACK, createNumber(i), OP_ADD, OP_READ_MEM });
				}

				// yank and drop it.
				st->program += makeinstr({ createNumber(sz), OP_YANK_STACK, OP_DROP });
			}
		};

		auto fetchUndecayedOperand = [&st](fir::Instruction* instr, size_t op) {
			iceAssert(op < instr->operands.size());

			fetchValue(st, instr->operands[op]);
		};

		auto fetchOperand = [&st, &decay](fir::Instruction* instr, size_t op) {
			iceAssert(op < instr->operands.size());

			decay(instr->operands[op]);
		};






		for(auto fn : st->firmod->getAllFunctions())
		{
			// printf("fn %s (id %zu)\n", fn->getName().str().c_str(), fn->id);

			if(fn->getBlockList().empty())
				continue;




			// st->program += strprintf("\n\n; function %s\n", fn->getName().str());


			// this one is for the real stack
			st->stackValues.clear();
			st->currentStackOffset = 0;
			st->numberOfLocalValues = 0;

			// this one is for the stack frame, ie. what lives in memory.
			st->stackFrameValueMap.clear();

			st->functionLocations[fn->id] = st->program.size();

			st->currentStackFrameSize = 0;
			for(auto t : fn->getStackAllocations())
				st->currentStackFrameSize += getSizeInWords(t);




			//* this is the function prologue! essentially
			//* push %rbp; mov %rsp, %rbp; sub $N, %rsp
			{
				// st->program += "\n; prologue\n";

				// now that we know how big the stack frame must be, we store the current stack pointer
				// (on the stack, just by reading from it)
				st->program += makeinstr({ createNumber(STACK_POINTER_IN_MEMORY), OP_READ_MEM });

				// then, we change the stack pointer. first, since the old value is already on the stack,
				// use 'F' to duplicate it.
				st->program += makeinstr({ CONST_0, OP_FETCH_STACK });

				// then, add our 'maxwatermark' to it.
				st->program += makeinstr({ createNumber(st->currentStackFrameSize), OP_ADD });

				// finally, store it into the pointer.
				st->program += makeinstr({ createNumber(STACK_POINTER_IN_MEMORY), OP_WRITE_MEM });

				// account for the base pointer on the stack
				st->currentStackOffset++;
			}



			// add the arguments. they are immutable, so we do not need to spill them to memory!
			for(auto arg : fn->getArguments())
			{
				// arguments were pushed in reverse order, meaning the first argument is now on the top of the stack.
				// they're already pushed, so we just track the offsets.

				// we do this manually, because we don't want to increment numberOfLocalValues --- only currentStackOffset.

				st->stackValues[arg->id] = st->currentStackOffset - 1;
				st->currentStackOffset += getSizeInWords(arg->getType());
			}




			int32_t currentStackWatermark = 0;
			auto allocStackMem = [&st, &currentStackWatermark](fir::Type* ty) -> int32_t {

				auto sz = getSizeInWords(ty);
				iceAssert(currentStackWatermark + sz <= st->currentStackFrameSize);

				auto ret = currentStackWatermark;
				currentStackWatermark += sz;

				return ret;
			};

			auto dropBlockLocals = [&st](int32_t ofs = 0) {

				for(size_t i = 0; i < st->numberOfLocalValues; i++)
					st->program += makeinstr({ createNumber(ofs), OP_YANK_STACK, OP_DROP });

				st->numberOfLocalValues = 0;
			};

			size_t blkIdx = 0;
			for(auto block : fn->getBlockList())
			{
				// printf("block %s (id %zu)\n", block->getName().str().c_str(), block->id);
				// st->program += strprintf("\n\n; block %s - %zu\n", block->getName().str(), st->program.size());

				st->basicBlockLocations[block->id] = st->program.size();

				for(auto inst : block->getInstructions())
				{
					st->temporaryStackValueOffset = 0;
					using fir::OpKind;

					switch(inst->opKind)
					{
						case OpKind::ArraySlice_GetData:
						case OpKind::ArraySlice_GetLength:
						{
							iceAssert(inst->operands.size() == 1);
							size_t slicesz = getSizeInWords(inst->operands[0]->getType());

							size_t ind = 0;
							if(inst->opKind == OpKind::ArraySlice_GetData)         ind = SLICE_DATA_INDEX;
							else if(inst->opKind == OpKind::ArraySlice_GetLength)  ind = SLICE_LENGTH_INDEX;

							// fetch the struct:
							fetchOperand(inst, 0);

							// then, fetch the N-1-ith bit of the stack, where N is the size of the slice (2 words), and i is the index
							//? note: implicit assumption that both data and length are one word in size
							st->program += makeinstr({ createNumber(slicesz - 1 - ind), OP_FETCH_STACK });

							// we need to get rid of the struct in memory now.
							for(size_t i = 0; i < slicesz; i++)
								st->program += makeinstr({ CONST_1, OP_YANK_STACK, OP_DROP });

							// we're done
							recordLocalOnStack(st, inst->realOutput);
							break;
						}



						case OpKind::ArraySlice_SetData:
						case OpKind::ArraySlice_SetLength:
						{
							iceAssert(inst->operands.size() == 2);
							size_t slicesz = getSizeInWords(inst->operands[0]->getType());

							size_t ind = 0;
							if(inst->opKind == OpKind::ArraySlice_GetData)         ind = SLICE_DATA_INDEX;
							else if(inst->opKind == OpKind::ArraySlice_GetLength)  ind = SLICE_LENGTH_INDEX;

							// fetch the struct:
							fetchOperand(inst, 0);

							// now, we loop over all words in the struct; if it's the index we're modifying, we push the new
							// value on the stack. if it's not, we use 'fetch' to get the existing value.
							for(size_t i = 0; i < slicesz; i++)
							{
								if(i == ind)    fetchOperand(inst, 1);
								else            st->program += makeinstr({ createNumber(slicesz - 1 - i), OP_FETCH_STACK });
							}

							// we need to get rid of the struct in memory now.
							for(size_t i = 0; i < slicesz; i++)
								st->program += makeinstr({ createNumber(slicesz), OP_YANK_STACK, OP_DROP });

							// we're done
							recordLocalOnStack(st, inst->realOutput);
							break;
						}




						// basically, pointer arithmetic --- precisely equivalent to PointerAddition and PointerSubtraction.
						case OpKind::Value_GetPointer:  // fallthrough

						// just some integer multiplication and add/sub.
						case OpKind::Value_PointerAddition:
						case OpKind::Value_PointerSubtraction:
						{
							iceAssert(inst->operands.size() == 2);
							auto typesz = getSizeInWords(inst->operands[0]->getType());

							// multiply the typesize (or -1*typesize if subtraction) with the offset, then add to the pointer.
							fetchOperand(inst, 1);
							st->program += makeinstr({ createNumber(typesz * (inst->opKind == OpKind::Value_PointerSubtraction ? -1 : 1)),
								OP_MULTIPLY });

							fetchOperand(inst, 0);
							st->program += makeinstr(OP_ADD);

							recordLocalOnStack(st, inst->realOutput);
							break;
						}


						// this one is less equivalent, and we really only use it for arrays.
						// after indexing the pointer, we need to index again. say we have gep([T]*, M, N), the first index will add
						// M*sizeof([T]), the second index will add N*sizeof(T)
						case OpKind::Value_GetGEP2:
						{
							iceAssert(inst->operands.size() == 3);

							auto ptrty = inst->operands[0]->getType();
							iceAssert(ptrty->isPointerType() && ptrty->getPointerElementType()->isArrayType());

							auto arraysz = getSizeInWords(ptrty->getPointerElementType());
							auto arrelmsz = getSizeInWords(ptrty->getPointerElementType()->getArrayElementType());

							// fetch the first level index, then multiply by the array size
							fetchOperand(inst, 1);
							st->program += makeinstr({ createNumber(arraysz), OP_MULTIPLY });

							// fetch the second level index, then add the element size
							fetchOperand(inst, 2);
							st->program += makeinstr({ createNumber(arrelmsz), OP_ADD });

							// fetch the actual pointer, and just add.
							fetchOperand(inst, 0);
							st->program += makeinstr(OP_ADD);

							// done
							recordLocalOnStack(st, inst->realOutput);
							break;
						}


						// all of these are basically no-ops for us.
						case OpKind::Cast_IntSize:
						case OpKind::Cast_PointerType:
						case OpKind::Cast_PointerToInt:
						case OpKind::Cast_IntToPointer:
						case OpKind::Cast_IntSignedness:
						{
							iceAssert(inst->operands.size() == 2);

							fetchOperand(inst, 0);  // the thing.
							recordLocalOnStack(st, inst->realOutput);
							break;
						}

						case OpKind::Value_WritePtr:
						{
							iceAssert(inst->operands.size() == 2);

							fetchOperand(inst, 0);  // val
							fetchOperand(inst, 1);  // ptr

							st->program += makeinstr(OP_WRITE_MEM);
							break;
						}

						case OpKind::Value_ReadPtr:
						{
							iceAssert(inst->operands.size() == 1);
							fetchOperand(inst, 0);  // ptr

							st->program += makeinstr(OP_READ_MEM);
							recordLocalOnStack(st, inst->realOutput);
							break;
						}

						case OpKind::Signed_Add:
						case OpKind::Signed_Sub:
						case OpKind::Signed_Mul:
						case OpKind::Signed_Div:
						case OpKind::Unsigned_Add:
						case OpKind::Unsigned_Sub:
						case OpKind::Unsigned_Mul:
						case OpKind::Unsigned_Div:
						{
							iceAssert(inst->operands.size() == 2);

							std::string op;
							switch(inst->opKind)
							{
								case OpKind::Signed_Add: case OpKind::Unsigned_Add:
									op = OP_ADD; break;
								case OpKind::Signed_Sub: case OpKind::Unsigned_Sub:
									op = OP_SUBTRACT; break;
								case OpKind::Signed_Mul: case OpKind::Unsigned_Mul:
									op = OP_MULTIPLY; break;
								case OpKind::Signed_Div: case OpKind::Unsigned_Div:
									op = OP_DIVIDE; break;
								default:
									iceAssert(0); break;
							}

							fetchOperand(inst, 0);  // a
							fetchOperand(inst, 1);  // b
							st->program += makeinstr(op);

							recordLocalOnStack(st, inst->realOutput);
							break;
						}

						case OpKind::Signed_Mod:
						case OpKind::Unsigned_Mod:
						{
							iceAssert(inst->operands.size() == 2);

							// a % b = a - (b*(a / b))

							fetchOperand(inst, 0);  // a
							fetchOperand(inst, 0);  // a (again)
							fetchOperand(inst, 1);  // b
							st->program += makeinstr(OP_DIVIDE);

							fetchOperand(inst, 1);  // b (again)
							st->program += makeinstr(OP_MULTIPLY);
							st->program += makeinstr(OP_SUBTRACT);

							recordLocalOnStack(st, inst->realOutput);
							break;
						}

						case OpKind::Branch_Cond:
						{
							iceAssert(inst->operands.size() == 3);

							fetchOperand(inst, 0);  // cond

							// we want to jump if 1, so just do that minus 1
							// (-1 != 0, so if it's 0 it won't jump)
							st->program += makeinstr({ CONST_1, OP_SUBTRACT });

							dropBlockLocals(1);

							addRelativeRelocation(st, inst->operands[1]);
							st->program += makeinstr(OP_JMP_REL_IF_ZERO);


							if(blkIdx + 1 < fn->getBlockList().size() && inst->operands[2]->id == fn->getBlockList()[blkIdx + 1]->id)
							{
								// woohoo, skip it
							}
							else
							{
								addRelativeRelocation(st, inst->operands[2]);
								st->program += makeinstr(OP_JMP_REL);
							}

							break;
						}

						case OpKind::Branch_UnCond:
						{
							iceAssert(inst->operands.size() == 1);

							dropBlockLocals();

							if(blkIdx + 1 < fn->getBlockList().size() && inst->operands[0]->id == fn->getBlockList()[blkIdx + 1]->id)
							{
								// woohoo, skip it
							}
							else
							{
								// sad.

								addRelativeRelocation(st, inst->operands[0]);
								st->program += makeinstr(OP_JMP_REL);
							}

							break;
						}

						case OpKind::Value_StackAlloc:
						case OpKind::Value_CreateLVal:
						{
							// st->program += "\n; create lvalue\n";

							iceAssert(inst->operands.size() == 1);
							fir::Type* ft = inst->operands[0]->getType();

							auto stackaddr = allocStackMem(ft);

							// small opt: only make the base address once, use 'F' to get it subsequently
							st->program += calcAddrInStackFrame(st, stackaddr);

							for(size_t i = 0; i < getSizeInWords(ft); i++)
							{
								auto ofs = createNumber(i);

								// write 0s.
								st->program += makeinstr({ CONST_0, CONST_1, OP_FETCH_STACK, ofs, OP_ADD, OP_WRITE_MEM });
							}

							// throw the thing away
							st->program += makeinstr(OP_DROP);

							st->stackFrameValueMap[inst->realOutput->id] = stackaddr;
							break;
						}

						case OpKind::Value_AddressOf:
						{
							iceAssert(inst->operands.size() == 1);

							fetchUndecayedOperand(inst, 0); // thing
							recordLocalOnStack(st, inst->realOutput);
							break;
						}

						case OpKind::Value_Dereference:
						{
							iceAssert(inst->operands.size() == 1);

							fetchOperand(inst, 0);  // thing
							recordLocalOnStack(st, inst->realOutput);
							break;
						}

						case OpKind::Value_Store:
						{
							iceAssert(inst->operands.size() == 2);

							// see how big it is..
							auto sz = getSizeInWords(inst->operands[0]->getType());


							fetchOperand(inst, 0);          // val
							fetchUndecayedOperand(inst, 1); // ptr

							for(size_t i = 0; i < sz; i++)
							{
								auto ofs = createNumber(i);

								// this is the offset of the 'current word' for multi-word values.
								auto valofs = createNumber(sz - i);

								st->program += makeinstr({ valofs, OP_FETCH_STACK, CONST_1, OP_FETCH_STACK, ofs, OP_ADD, OP_WRITE_MEM });
							}

							st->program += makeinstr(OP_DROP);

							for(size_t i = 0; i < sz; i++) // drop the value also
								st->program += makeinstr(OP_DROP);

							break;
						}

						case OpKind::Value_CallFunction:
						{
							iceAssert(inst->operands.size() >= 1);

							fir::Function* fn = dcast(fir::Function, inst->operands[0]);
							iceAssert(fn);

							if(fn->isIntrinsicFunction())
							{
								if(fn->getName().str() == INTRINSIC_PRINT_CHAR)
								{
									iceAssert(inst->operands.size() == 2);

									fetchOperand(inst, 1);  // arg
									st->program += makeinstr(OP_PRINT_CHAR);
								}
								else if(fn->getName().str() == INTRINSIC_PRINT_INT)
								{
									iceAssert(inst->operands.size() == 2);

									fetchOperand(inst, 1);  // arg
									st->program += makeinstr(OP_PRINT_INT);
								}
								else if(fn->getName().str() == INTRINSIC_HALT)
								{
									st->program += makeinstr(OP_HALT);
								}
								else if(fn->getName().str() == INTRINSIC_DUMP_MEMORY)
								{
									st->program += makeinstr("!");
								}
								else if(fn->getName().str() == INTRINSIC_DUMP_STACK)
								{
									st->program += makeinstr("?");
								}
								else
								{
									error("unknown intrinsic '%s'", fn->getName().str());
								}
							}
							else
							{
								// push the arguments
								for(size_t i = 1; i < inst->operands.size(); i++)
									fetchOperand(inst, i);

								addRelocation(st, inst->operands[0]);
								st->program += makeinstr(OP_CALL);

								// we just pop the arguments here again -- cdecl is caller-cleanup
								for(size_t i = 1; i < inst->operands.size(); i++)
								{
									// problem: the arguments that we pushed are currently behind the return value
									// solution: use the fetch-and-delete (H) to grab them, then drop them.
									st->program += makeinstr({ createNumber(getSizeInWords(fn->getReturnType())), OP_YANK_STACK, OP_DROP });
								}
							}



							if(!fn->getReturnType()->isVoidType())
								recordLocalOnStack(st, inst->realOutput);

							break;
						}

						case OpKind::Value_Return:
						{
							if(inst->operands.size() > 0)
							{
								iceAssert(inst->operands.size() == 1);

								// just push the value.
								fetchOperand(inst, 0);
							}

							size_t returnValueSize = getSizeInWords(fn->getReturnType());

							// similar deal -- in certain cases (eg. `return foo()`), the local value that we 'recorded' is right on top
							// of the stack. we can't drop it yet, because we need it! so, push a copy first, then yank the locals from
							// behind using 'H' and drop them.

							dropBlockLocals(returnValueSize);

							//* this is the function epilogue
							{
								//* mov %rbp, %rsp; pop %rbp
								{
									// so what we do is just restore the value on the stack, which, barring any suspicious things, should
									// still be there -- but behind any return values.

									st->program += makeinstr({ createNumber(returnValueSize), OP_YANK_STACK });

									// now it's at the top -- we write that to the stack pointer place.
									st->program += makeinstr({ createNumber(STACK_POINTER_IN_MEMORY), OP_WRITE_MEM });
								}
							}

							st->program += makeinstr(OP_RETURN);
							break;
						}



						case OpKind::ICompare_Multi:
						{
							// this is dead simple -- basically just 'J'.
							// but i don't think we emit this from flax just yet.
							iceAssert(inst->operands.size() == 2);

							fetchOperand(inst, 0);  // a
							fetchOperand(inst, 1);  // b
							st->program += makeinstr(OP_COMPARE);
							recordLocalOnStack(st, inst->realOutput);
							break;
						}

						case OpKind::ICompare_Equal:
						{
							iceAssert(inst->operands.size() == 2);

							/*
								<A><B>SdZabGb, which is essentially this:

								sub <A>, <B>
								jz true
								push 0
								jmp merge
								true: push 1
								merge:
							*/

							fetchOperand(inst, 0);  // a
							fetchOperand(inst, 1);  // b
							st->program += makeinstr({ OP_SUBTRACT, CONST_3, OP_JMP_REL_IF_ZERO, CONST_0, CONST_1, OP_JMP_REL, CONST_1 });
							recordLocalOnStack(st, inst->realOutput);

							break;
						}

						case OpKind::ICompare_NotEqual:
						{
							iceAssert(inst->operands.size() == 2);

							// similar to icmpeq, but we just swap the 0 and the 1 constant.
							fetchOperand(inst, 0);  // a
							fetchOperand(inst, 1);  // b
							st->program += makeinstr({ OP_SUBTRACT, CONST_3, OP_JMP_REL_IF_ZERO, CONST_1, CONST_1, OP_JMP_REL, CONST_0 });
							recordLocalOnStack(st, inst->realOutput);
							break;
						}

						case OpKind::ICompare_Greater:
						{
							// <B><A>cGeGJgMGaeGaaab
							// what this is doing, is multiplying the result of J with some constant, so it either
							// jumps forward, backwards, or nowhere, depending on the result. then, we just push
							// the appropriate constants depending on the result.
							// it's a more general form of SdZabGb that we used for == and !=.
							iceAssert(inst->operands.size() == 2);

							fetchOperand(inst, 0);  // a
							fetchOperand(inst, 1);  // b
							st->program += makeinstr({ CONST_2, OP_JMP_REL, CONST_4, OP_JMP_REL, OP_COMPARE, CONST_6,
								OP_MULTIPLY, OP_JMP_REL, CONST_0,  // <<< this changes
								CONST_4, OP_JMP_REL, CONST_0, CONST_0, CONST_0, CONST_1 });
							recordLocalOnStack(st, inst->realOutput);
							break;
						}

						case OpKind::ICompare_Less:
						{
							iceAssert(inst->operands.size() == 2);

							// we just swap the order of operands.

							fetchOperand(inst, 1);  // b
							fetchOperand(inst, 0);  // a
							st->program += makeinstr({ CONST_2, OP_JMP_REL, CONST_4, OP_JMP_REL, OP_COMPARE, CONST_6,
								OP_MULTIPLY, OP_JMP_REL, CONST_0,  // <<< this changes
								CONST_4, OP_JMP_REL, CONST_0, CONST_0, CONST_0, CONST_1 });
							recordLocalOnStack(st, inst->realOutput);
							break;
						}

						case OpKind::ICompare_GreaterEqual:
						{
							iceAssert(inst->operands.size() == 2);

							// take the < version, and invert the outputs
							fetchOperand(inst, 1);  // b
							fetchOperand(inst, 0);  // a
							st->program += makeinstr({ CONST_2, OP_JMP_REL, CONST_4, OP_JMP_REL, OP_COMPARE, CONST_6,
								OP_MULTIPLY, OP_JMP_REL, CONST_1,  // <<< this changes
								CONST_4, OP_JMP_REL, CONST_0, CONST_0, CONST_0, CONST_0 });
							recordLocalOnStack(st, inst->realOutput);
							break;
						}

						case OpKind::ICompare_LessEqual:
						{
							iceAssert(inst->operands.size() == 2);

							// take the > version, and invert the outputs
							fetchOperand(inst, 0);  // a
							fetchOperand(inst, 1);  // b
							st->program += makeinstr({ CONST_2, OP_JMP_REL, CONST_4, OP_JMP_REL, OP_COMPARE, CONST_6,
								OP_MULTIPLY, OP_JMP_REL, CONST_1,  // <<< this changes
								CONST_4, OP_JMP_REL, CONST_0, CONST_0, CONST_0, CONST_0 });
							recordLocalOnStack(st, inst->realOutput);
							break;
						}



						case OpKind::Unreachable:
						{
							st->program += makeinstr(OP_HALT);
							break;
						}

						case OpKind::Invalid:
							error("invalid opcode!");
							break;

						default:
							warn("unhandled: '%s'", inst->str());
							break;
					}
				}

				blkIdx++;
			}
		}

















		{
			std::string tmp;
			for(const auto& mi : st->memoryInitialisers)
				tmp += mi;

			// tmp += "!";

			st->program = (tmp + st->program);
			st->relocationOffset = tmp.size();

			auto relocate = [&st](int32_t _instr, size_t target, int32_t origin) {

				auto instr = st->relocationOffset + _instr;

				// expect the relocation to be unfilled!
				if(st->program.find(EMPTY_RELOCATION, instr) != instr)
					error("wtf? '%s'", st->program.substr(instr, 32));

				int32_t loc = 0;
				if(auto it = st->functionLocations.find(target); it != st->functionLocations.end())
					loc = it->second;

				else if(auto it = st->basicBlockLocations.find(target); it != st->basicBlockLocations.end())
					loc = it->second;

				else
					error("no relocation for value id %zu", target);

				loc -= origin;
				loc += (origin != 0 ? 0 : st->relocationOffset);

				// printf("relocation of id %zu from prog %d is %d\n", target, origin, loc);


				auto str = createNumber(loc);

				if(str.size() > MAX_RELOCATION_SIZE)
				{
					error("size of constant '%d' exceeds maximum relocation size (%d); generated string was '%s'",
						loc, MAX_RELOCATION_SIZE, str);
				}

				if(str.size() < MAX_RELOCATION_SIZE)
					str += std::string(MAX_RELOCATION_SIZE - str.size(), ' ');

				iceAssert(str.size() == MAX_RELOCATION_SIZE);
				st->program.replace(instr, MAX_RELOCATION_SIZE, str);
			};



			// handle relocations.
			for(auto [ _instr, target ] : st->relocations)
				relocate(_instr, target, 0);

			for(auto [ _instr, target ] : st->relativeRelocations)
				relocate(_instr, target.first, target.second);
		}

		this->program = st->program;
	}












	LSCVMBackend::LSCVMBackend(CompiledData& dat, std::vector<std::string> inputs, std::string output)
		: Backend(BackendCaps::EmitAssembly | BackendCaps::EmitProgram | BackendCaps::JIT, dat, inputs, output)
	{
	}

	void LSCVMBackend::writeOutput()
	{
		if(frontend::getOutputMode() == ProgOutputMode::RunJit)
		{
			printf("\ncompiled program (%#zx bytes):\n\n", this->program.size());
			printf("%s\n\n", this->program.c_str());

			// this->program += "?!";
			this->executeProgram(this->program);
		}
		else
		{
			auto out = std::ofstream(this->outputFilename, std::ios::out);
			out << this->program;

			out.close();
		}
	}

	void LSCVMBackend::optimiseProgram()
	{
		// lol what?
	}

	std::string LSCVMBackend::str()
	{
		return "LSCVM";
	}
}








/*
	todo list:

	// Invalid
	// Signed_Add
	// Signed_Sub
	// Signed_Mul
	// Signed_Div
	// Signed_Mod
	// Signed_Neg
	// Unsigned_Add
	// Unsigned_Sub
	// Unsigned_Mul
	// Unsigned_Div
	// Unsigned_Mod
	// ICompare_Equal
	// ICompare_NotEqual
	// ICompare_Greater
	// ICompare_Less
	// ICompare_GreaterEqual
	// ICompare_LessEqual
	// ICompare_Multi
	// Branch_UnCond
	// Branch_Cond
	// Unreachable
	// Value_Store
	// Value_CreateLVal
	// * Value_CallFunction
	// * Value_Return
	// Value_ReadPtr
	// Value_WritePtr
	// Value_StackAlloc
	// Cast_PointerType
	// Cast_PointerToInt
	// Cast_IntToPointer
	// Value_Dereference
	// Value_AddressOf
	// Value_PointerAddition
	// Value_PointerSubtraction
	// Value_GetPointer
	// Value_GetGEP2
	// ArraySlice_GetData
	// ArraySlice_SetData
	// ArraySlice_GetLength
	// ArraySlice_SetLength
	Value_GetStructMember
	Value_GetPointerToStructMember
	Value_InsertValue
	Value_ExtractValue
	Value_Select
	Value_CreatePHI
	* Value_CallFunctionPointer
	* Value_CallVirtualMethod
	? Bitwise_Not
	? Bitwise_Xor
	? Bitwise_Arithmetic_Shr
	? Bitwise_Logical_Shr
	? Bitwise_Shl
	? Bitwise_And
	? Bitwise_Or
	Cast_Bitcast
	Cast_IntSize
	Cast_Signedness
	Cast_IntSignedness
	Integer_ZeroExt
	Integer_Truncate
	Logical_Not
	Misc_Sizeof
	SAA_GetData
	SAA_SetData
	SAA_GetLength
	SAA_SetLength
	SAA_GetCapacity
	SAA_SetCapacity
	SAA_GetRefCountPtr
	SAA_SetRefCountPtr
	Any_GetData
	Any_SetData
	Any_GetTypeID
	Any_SetTypeID
	Any_GetRefCountPtr
	Any_SetRefCountPtr
	Range_GetLower
	Range_SetLower
	Range_GetUpper
	Range_SetUpper
	Range_GetStep
	Range_SetStep
	Enum_GetIndex
	Enum_SetIndex
	Enum_GetValue
	Enum_SetValue
	Union_SetValue
	Union_GetValue
	Union_GetVariantID
	Union_SetVariantID
	RawUnion_GEP
*/






















