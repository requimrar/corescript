// operators.cpp
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"

#define dcast(t, v)		dynamic_cast<t*>(v)

static bool isBuiltinType(fir::Type* ty)
{
	return (ty->isConstantNumberType()
		|| ty->isDynamicArrayType()
		|| ty->isArraySliceType()
		|| ty->isPrimitiveType()
		|| ty->isFunctionType()
		|| ty->isPointerType()
		|| ty->isStringType()
		|| ty->isRangeType()
		|| ty->isArrayType()
		|| ty->isVoidType()
		|| ty->isNullType()
		|| ty->isCharType()
		|| ty->isBoolType());
}


sst::Stmt* ast::OperatorOverloadDefn::typecheck(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this->loc);
	defer(fs->popLoc());

	if(this->kind == Kind::Invalid)
		error(this, "Invalid operator kind; must be one of 'infix', 'postfix', or 'prefix'");

	if(fs->isInStructBody())
		error(this, "Operator overloads cannot be methods of a type.");

	this->generateDeclaration(fs, infer);

	// call the superclass method.
	return this->ast::FuncDefn::typecheck(fs, infer);
}

void ast::OperatorOverloadDefn::generateDeclaration(sst::TypecheckState* fs, fir::Type* infer)
{
	fs->pushLoc(this->loc);
	defer(fs->popLoc());

	// there's nothing different.
	this->ast::FuncDefn::generateDeclaration(fs, infer);

	auto defn = this->generatedDefn;
	iceAssert(defn);

	// ok, do our checks on the defn instead.
	auto ft = defn->type->toFunctionType();

	if(this->kind == Kind::Infix)
	{
		if(ft->getArgumentTypes().size() != 2)
		{
			error(this, "Operator overload for binary operator '%s' must have exactly 2 parameters, but %d %s found",
				operatorToString(this->op), ft->getArgumentTypes().size(), ft->getArgumentTypes().size() == 1 ? "was" : "were");
		}
		else if(!isAssignOp(this->op) && isBuiltinType(ft->getArgumentN(0)) && isBuiltinType(ft->getArgumentN(1)))
		{
			exitless_error(this, "Binary operator overload (for operator '%s') cannot take two builtin types as arguments (have '%s' and '%s')",
				operatorToString(this->op), ft->getArgumentN(0), ft->getArgumentN(1));

			info("At least one of the parameters must be a user-defined type");
			doTheExit();
		}
	}
	else if(this->kind == Kind::Postfix || this->kind == Kind::Prefix)
	{
		if(ft->getArgumentTypes().size() != 1)
		{
			error(this, "Operator overload for unary operator '%s' must have exactly 1 parameter, but %d %s found",
				operatorToString(this->op), ft->getArgumentTypes().size(), ft->getArgumentTypes().size() == 1 ? "was" : "were");
		}
		else if(isBuiltinType(ft->getArgumentN(0)))
		{
			error(defn->arguments[0], "Unary operator '%s' cannot be overloaded for the builtin type '%s'",
				operatorToString(this->op), ft->getArgumentN(0));
		}
	}

	// ok, further checks.
	if(isAssignOp(this->op))
	{
		if(!ft->getReturnType()->isVoidType())
		{
			error(this, "Operator overload for assignment operators (have '%s') must return void, but a return type of '%s' was found",
				operatorToString(this->op), ft->getReturnType());
		}
		else if(!ft->getArgumentN(0)->isPointerType())
		{
			error(defn->arguments[0], "Operator overload for assignment operator '%s' must take a pointer to the type as the first parameter, found '%s'",
				operatorToString(this->op), ft->getArgumentN(0));
		}
		else if(isBuiltinType(ft->getArgumentN(0)->getPointerElementType()))
		{
			error(defn->arguments[0], "Assignment operator '%s' cannot be overloaded for the builtin type '%s'",
				operatorToString(this->op), ft->getArgumentN(0));
		}
	}

	// before we add, check for duplication.
	auto& thelist = (this->kind == Kind::Infix ? fs->stree->infixOperatorOverloads : (this->kind == Kind::Prefix
		? fs->stree->prefixOperatorOverloads : fs->stree->postfixOperatorOverloads));

	for(auto it : thelist[this->op])
	{
		if(fs->isDuplicateOverload(it->params, defn->params))
		{
			exitless_error(this, "Duplicate operator overload for '%s' taking identical arguments", operatorToString(this->op));
			info(it, "Previous definition was here:");

			doTheExit();
		}
	}

	// ok, we should be good now.
	thelist[this->op].push_back(defn);
}

























