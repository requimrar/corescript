// enums.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"

#include "memorypool.h"

/*
	ok, here's some documentation of how enumerations work
	they're basically strong enums, unlike C enums.
	you can cast them to ints, and they'll be numbered appropriately starting at 0.

	each enumeration value is a struct: { index: i64, value: $T }
	the index is the index of the thing, and allows some runtime things on the enum;

	for example, a value of type enumeration can have a .name, and in the future can simplify
	runtime type information getting by having a simpler array-index mechanism.

	so, a fir::EnumType will now be like a StringType, opaque-ish thing. we'll have IRB things in a similar
	fashion (it'll be a value type), and that's about it.
*/


TCResult ast::EnumDefn::generateDeclaration(sst::TypecheckState* fs, fir::Type* infer, const TypeParamMap_t& gmaps)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto [ success, ret ] = this->checkForExistingDeclaration(fs, gmaps);
	if(!success)    return TCResult::getParametric();
	else if(ret)    return TCResult(ret);

	auto defnname = util::typeParamMapToString(this->name, gmaps);
	auto defn = util::pool<sst::EnumDefn>(this->loc);
	defn->bareName = this->name;

	defn->id = Identifier(defnname, IdKind::Type);
	defn->id.scope = this->enclosingScope;
	defn->visibility = this->visibility;
	defn->original = this;
	defn->enclosingScope = this->enclosingScope;
	defn->innerScope = this->enclosingScope.appending(defnname);

	// set it to void first, because we want to defer typechecking the member type.
	defn->type = fir::EnumType::get(defn->id.convertToName(), fir::Type::getVoid());

	if(auto err = fs->checkForShadowingOrConflictingDefinition(defn, [](auto, auto) -> bool { return true; }))
		return TCResult(err);

	defn->enclosingScope.stree->addDefinition(defnname, defn, gmaps);

	this->genericVersions.push_back({ defn, fs->getGenericContextStack() });
	return TCResult(defn);
}


TCResult ast::EnumDefn::typecheck(sst::TypecheckState* fs, fir::Type* infer, const TypeParamMap_t& gmaps)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto tcr = this->generateDeclaration(fs, infer, gmaps);
	if(tcr.isParametric())  return tcr;
	else if(tcr.isError())  error(this, "failed to generate declaration for enum '%s'", this->name);

	auto defn = dcast(sst::EnumDefn, tcr.defn());
	iceAssert(defn);

	fs->teleportInto(defn->innerScope);

	if(this->memberType)	defn->memberType = fs->convertParserTypeToFIR(this->memberType);
	else					defn->memberType = fir::Type::getNativeWord();

	auto ety = defn->type->toEnumType();
	iceAssert(ety);
	ety->setCaseType(defn->memberType);

	size_t index = 0;
	for(auto cs : this->cases)
	{
		sst::Expr* val = 0;
		if(cs.value)
		{
			iceAssert(defn->memberType);
			val = cs.value->typecheck(fs, defn->memberType).expr();

			if(val->type != defn->memberType)
				error(cs.value, "mismatched type in enum case value; expected type '%s', but found type '%s'", defn->memberType, val->type);
		}

		auto ecd = util::pool<sst::EnumCaseDefn>(cs.loc);
		ecd->id = Identifier(cs.name, IdKind::Name);
		ecd->id.scope = fs->scope();
		ecd->type = ety;
		ecd->parentEnum = defn;
		ecd->val = val;
		ecd->index = index++;

		defn->cases[cs.name] = ecd;
		fs->stree->addDefinition(cs.name, ecd);
	}

	defn->type = ety;

	fs->teleportOut();
	return TCResult(defn);
}





