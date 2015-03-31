// TypeInfoGeneration.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../include/ast.h"
#include "../include/codegen.h"
#include "../include/llvm_all.h"

using namespace Ast;
using namespace Codegen;

namespace TypeInfo
{
	void addNewType(CodegenInstance* cgi, llvm::Type* stype, StructBase* str, ExprKind etype)
	{
		(void) str;
		if(dynamic_cast<Enumeration*>(str) && cgi->isEnum(stype) && etype == ExprKind::Enum && str->name == "Type")
			return;


		for(auto k : cgi->rootNode->typeList)
		{
			if(k.first == stype->getStructName())
				return;
		}

		cgi->rootNode->typeList.push_back(std::make_pair(stype->getStructName(), etype));
	}

	// this adds the type info for all the basic types
	void initialiseTypeInfo(CodegenInstance* cgi)
	{
		(void) cgi;
	}

	void generateTypeInfo(CodegenInstance* cgi)
	{
		if(cgi->rootNode->typeList.size() > 0)
		{
			Enumeration* enr = new Enumeration(Parser::PosInfo(), "Type");
			enr->isStrong = true;

			Number* num = new Number(Parser::PosInfo(), (int64_t) 1);

			for(std::pair<std::string, ExprKind> pair : cgi->rootNode->typeList)
			{
				enr->cases.push_back(std::make_pair(pair.first, num));

				num = new Number(Parser::PosInfo(), num->ival + 1);
			}

			// note: Enumeration does nothing in codegen()
			// fix this if that happens to change in the future.
			enr->createType(cgi);
		}
	}
}







