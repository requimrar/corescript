// codegen.h
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once
#include "ast.h"
#include "llvm_all.h"
#include "typeinfo.h"
#include "utf8rewind.h"

#include <vector>



enum class SymbolType
{
	Generic,
	Function,
	Variable,
	Type
};

namespace GenError
{
	void unknownSymbol(Codegen::CodegenInstance* cgi, Ast::Expr* e, std::string symname, SymbolType st) __attribute__((noreturn));
	void useAfterFree(Codegen::CodegenInstance* cgi, Ast::Expr* e, std::string symname);
	void duplicateSymbol(Codegen::CodegenInstance* cgi, Ast::Expr* e, std::string symname, SymbolType st) __attribute__((noreturn));
	void noOpOverload(Codegen::CodegenInstance* cgi, Ast::Expr* e, std::string type, Ast::ArithmeticOp op) __attribute__((noreturn));
	void invalidAssignment(Codegen::CodegenInstance* cgi, Ast::Expr* e, llvm::Value* a, llvm::Value* b) __attribute__((noreturn));
	void invalidAssignment(Codegen::CodegenInstance* cgi, Ast::Expr* e, llvm::Type* a, llvm::Type* b) __attribute__((noreturn));
	void nullValue(Codegen::CodegenInstance* cgi, Ast::Expr* e, int funcArgument = -1) __attribute__((noreturn));

	void invalidInitialiser(Codegen::CodegenInstance* cgi, Ast::Expr* e, Ast::Struct* str,
		std::vector<llvm::Value*> args) __attribute__((noreturn));

	void expected(Codegen::CodegenInstance* cgi, Ast::Expr* e, std::string exp) __attribute__((noreturn));
}

namespace Codegen
{
	struct CodegenInstance
	{
		// todo: hack
		bool isStructCodegen = false;

		Ast::Root* rootNode;
		llvm::Module* mainModule;
		llvm::FunctionPassManager* Fpm;
		std::deque<SymTab_t> symTabStack;
		llvm::ExecutionEngine* execEngine;
		std::deque<BracedBlockScope> blockStack;
		std::deque<Ast::Func*> funcStack;
		std::deque<std::string> namespaceStack;
		std::deque<std::deque<std::string>> importedNamespaces;

		std::vector<std::string> rawLines;

		TypeMap_t typeMap;
		FuncMap_t funcMap;

		llvm::IRBuilder<> mainBuilder = llvm::IRBuilder<>(llvm::getGlobalContext());







		// "block" scopes, ie. breakable bodies (loops)
		void pushBracedBlock(Ast::BreakableBracedBlock* block, llvm::BasicBlock* body, llvm::BasicBlock* after);
		BracedBlockScope* getCurrentBracedBlockScope();
		Ast::Func* getCurrentFunctionScope();
		void setCurrentFunctionScope(Ast::Func* f);
		void clearCurrentFunctionScope();
		void popBracedBlock();

		// normal scopes, ie. variable scopes within braces
		void pushScope();
		void pushScope(SymTab_t tab);
		SymTab_t& getSymTab();
		bool isDuplicateSymbol(const std::string& name);
		llvm::Value* getSymInst(Ast::Expr* user, const std::string& name);
		SymbolPair_t* getSymPair(Ast::Expr* user, const std::string& name);
		Ast::VarDecl* getSymDecl(Ast::Expr* user, const std::string& name);
		void addSymbol(std::string name, llvm::Value* ai, Ast::VarDecl* vardecl);
		void popScope();
		void clearScope();

		// function scopes: namespaces, nested functions.
		void pushNamespaceScope(std::string namespc);
		bool isValidNamespace(std::string namespc);

		void addFunctionToScope(std::string name, FuncPair_t func);
		void addNewType(llvm::Type* ltype, Ast::StructBase* atype, TypeKind e);
		bool isDuplicateFuncDecl(std::string name);

		void removeType(std::string name);
		TypePair_t* getType(std::string name);
		TypePair_t* getType(llvm::Type* type);
		FuncPair_t* getDeclaredFunc(std::string name);
		FuncPair_t* getDeclaredFunc(Ast::FuncCall* fc);

		void clearNamespaceScope();
		void popNamespaceScope();


		// llvm::Types for non-primitive (POD) builtin types (string)
		void applyExtensionToStruct(std::string extName);

		llvm::Type* getLlvmType(Ast::Expr* expr);
		llvm::Type* getLlvmType(Ast::Expr* user, Ast::ExprType type);
		void autoCastType(llvm::Type* target, llvm::Value*& right, llvm::Value* rhsPtr = 0);
		void autoCastType(llvm::Value* left, llvm::Value*& right, llvm::Value* rhsPtr = 0);


		bool isPtr(Ast::Expr* e);
		bool isEnum(Ast::ExprType type);
		bool isEnum(llvm::Type* type);
		bool isArrayType(Ast::Expr* e);
		bool isSignedType(Ast::Expr* e);
		bool isBuiltinType(Ast::Expr* e);
		bool isIntegerType(Ast::Expr* e);
		bool isBuiltinType(llvm::Type* e);
		bool isTypeAlias(Ast::ExprType type);
		bool isTypeAlias(llvm::Type* type);
		bool isAnyType(llvm::Type* type);

		bool isDuplicateType(std::string name);

		llvm::Value* lastMinuteUnwrapType(Ast::Expr* user, llvm::Value* alloca);


		std::string mangleRawNamespace(std::string original);
		std::string mangleWithNamespace(std::string original, bool isFunction = true);
		std::string mangleWithNamespace(std::string original, std::deque<std::string> ns, bool isFunction = true);

		std::string mangleMemberFunction(Ast::StructBase* s, std::string orig, std::deque<Ast::Expr*> args);
		std::string mangleMemberFunction(Ast::StructBase* s, std::string orig, std::deque<Ast::Expr*> args, std::deque<std::string> ns);
		std::string mangleMemberFunction(Ast::StructBase* s, std::string orig, std::deque<Ast::VarDecl*> args, std::deque<std::string> ns,
			bool isStatic = false);

		std::string mangleName(Ast::StructBase* s, std::string orig);
		std::string mangleName(Ast::StructBase* s, Ast::FuncCall* fc);
		std::string mangleName(std::string base, std::deque<Ast::Expr*> args);
		std::string mangleName(std::string base, std::deque<llvm::Type*> args);
		std::string mangleName(std::string base, std::deque<Ast::VarDecl*> args);


		std::string getReadableType(Ast::Expr* expr);
		std::string getReadableType(llvm::Type* type);
		std::string getReadableType(llvm::Value* val);


		llvm::AllocaInst* allocateInstanceInBlock(Ast::VarDecl* var);
		llvm::AllocaInst* allocateInstanceInBlock(llvm::Type* type, std::string name = "");


		llvm::Type* parseTypeFromString(Ast::Expr* user, std::string type);
		std::string unwrapPointerType(std::string type, int* indirections);

		Ast::Func* getFunctionFromStructFuncCall(Ast::StructBase* str, Ast::FuncCall* fc);
		Ast::Expr* recursivelyResolveNested(Ast::MemberAccess* base, std::deque<std::string>* scopes = 0);
		Ast::Struct* getNestedStructFromScopes(Ast::Expr* user, std::deque<std::string> scopes);


		Ast::Result_t doBinOpAssign(Ast::Expr* user, Ast::Expr* l, Ast::Expr* r, Ast::ArithmeticOp op, llvm::Value* lhs, llvm::Value* ref, llvm::Value* rhs, llvm::Value* rhsPtr);


		Ast::Result_t assignValueToAny(llvm::Value* lhsPtr, llvm::Value* rhs, llvm::Value* rhsPtr);
		Ast::Result_t extractValueFromAny(llvm::Type* type, llvm::Value* ptr);


		void evaluateDependencies(Ast::Expr* expr);


		Ast::Root* getRootAST();
		llvm::LLVMContext& getContext();
		llvm::Value* getDefaultValue(Ast::Expr* e);
		bool verifyAllPathsReturn(Ast::Func* func, size_t* stmtCounter, bool checkType);

		llvm::Type* getLlvmTypeOfBuiltin(std::string type);
		Ast::ArithmeticOp determineArithmeticOp(std::string ch);
		llvm::Instruction::BinaryOps getBinaryOperator(Ast::ArithmeticOp op, bool isSigned, bool isFP);
		llvm::Function* getStructInitialiser(Ast::Expr* user, TypePair_t* pair, std::vector<llvm::Value*> args);
		Ast::Result_t doPointerArithmetic(Ast::ArithmeticOp op, llvm::Value* lhs, llvm::Value* lhsptr, llvm::Value* rhs);
		Ast::Result_t callOperatorOnStruct(TypePair_t* pair, llvm::Value* self, Ast::ArithmeticOp op, llvm::Value* val, bool fail = true);
	};

	Ast::Result_t enumerationAccessCodegen(CodegenInstance* cgi, Ast::Expr* lhs, Ast::Expr* rhs);
	void doCodegen(std::string filename, Ast::Root* root, CodegenInstance* cgi);
	void writeBitcode(std::string filename, CodegenInstance* cgi);
}


void error(const char* msg, ...) __attribute__((noreturn));
void error(Ast::Expr* e, const char* msg, ...) __attribute__((noreturn));
void error(Codegen::CodegenInstance* cgi, Ast::Expr* e, const char* msg, ...) __attribute__((noreturn));

void warn(const char* msg, ...);
void warn(Ast::Expr* e, const char* msg, ...);
void warn(Codegen::CodegenInstance* cgi, Ast::Expr* e, const char* msg, ...);


#define __nothing
#define iceAssert(x)		((x) ? (void) (0) : error("Compiler assertion at %s:%d, cause:\n%s", __FILE__, __LINE__, #x))



