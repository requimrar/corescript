// execute.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#include "repl.h"
#include "parser.h"
#include "frontend.h"
#include "parser_internal.h"

#include "codegen.h"
#include "typecheck.h"

#include "ir/module.h"
#include "ir/interp.h"
#include "ir/irbuilder.h"

#include "memorypool.h"

// defined in codegen/directives.cpp
fir::ConstantValue* magicallyRunExpressionAtCompileTime(cgn::CodegenState* cs, sst::Stmt* stmt, fir::Type* infer,
	const fir::Name& fname, fir::interp::InterpState* is = 0);

namespace repl
{
	struct State
	{
		State()
		{
			auto modname = "__repl_mod__";

			this->module = new fir::Module(modname);

			sst::StateTree* tree = new sst::StateTree(modname, 0);
			this->fs = new sst::TypecheckState(tree);
			this->cs = new cgn::CodegenState(fir::IRBuilder(this->module));
			this->cs->module = this->module;


			this->interpState = new fir::interp::InterpState(this->module);
			this->interpState->initialise(/* runGlobalInit: */ true);

			// so we don't crash, give us a starting location.
			this->cs->pushLoc(Location());
		}

		~State()
		{
			delete this->interpState;
			delete this->cs;
			delete this->fs;
			delete this->module;
		}

		fir::Module* module;
		cgn::CodegenState* cs;
		sst::TypecheckState* fs;
		fir::interp::InterpState* interpState;

		size_t fnCounter = 0;
		size_t varCounter = 0;
	};

	static State* state = 0;
	void setupEnvironment()
	{
		if(state)
			delete state;

		state = new State();
	}

	void setEnvironment(State* st)
	{
		state = st;
	}

	State* getEnvironment()
	{
		return state;
	}


	std::optional<sst::Stmt*> parseAndTypecheck(const std::string& line, bool* needmore)
	{
		std::string replName = "<repl>";

		frontend::CollectorState collector;

		// lex.
		platform::cachePreExistingFile(replName, line);
		auto lexResult = frontend::lexTokensFromString(replName, line);

		// parse, but first setup the environment.
		auto st = parser::State(lexResult.tokens);
		auto _stmt = parser::parseStmt(st, /* exprs: */ true);

		*needmore = false;
		if(_stmt.needsMoreTokens())
		{
			*needmore = true;
			return std::nullopt;
		}
		else if(_stmt.isError())
		{
			_stmt.err()->post();

			return std::nullopt;
		}
		else
		{
			auto stmt = _stmt.val();

			// ugh.
			auto tcr = TCResult(reinterpret_cast<sst::Stmt*>(0));

			// there's no need to fiddle with AST-level trees -- once we typecheck it,
			// it will store the relevant state into the TypecheckState.

			try
			{
				// note: usually, visitDeclarables in the top-level typecheck will set the realScope.
				// BUT, since we're not doing that, we must set it manually!
				if(auto def = dcast(ast::Parameterisable, stmt); def)
					def->enclosingScope = state->fs->scope();

				tcr = stmt->typecheck(state->fs);
			}
			catch(ErrorException& ee)
			{
				ee.err->post();
				printf("\n");

				return std::nullopt;
			}

			if(tcr.isError())
			{
				tcr.error()->post();
				printf("\n");
			}
			else if(!tcr.isParametric() && !tcr.isDummy())
			{
				return tcr.stmt();
			}

			return std::nullopt;
		}
	}


	bool processLine(const std::string& line)
	{
		// before we begin, bring us into a new namespace.
		state->fs->pushAnonymousTree();

		bool needmore = false;
		auto stmt = repl::parseAndTypecheck(line, &needmore);
		if(!stmt)
			return needmore;


		{
			// copy some stuff over.
			state->cs->typeDefnMap = state->fs->typeDefnMap;

			// so the thing is, all the previous things have already been code-generated,
			// and have had their initialisers run. so there's really no need for their
			// init pieces to stick around. we need to remove the functions as well for this
			// to work properly!
			for(auto [ gv, pc ] : state->cs->globalInitPieces)
			{
				state->module->removeFunction(pc);
				delete pc;
			}

			state->cs->globalInitPieces.clear();

			// ok, we have a thing. try to run it. note: this will help us to run is->initialise(true),
			// which will call the global inits. this function also calls Stmt::codegen, which will
			// (potentially) populate the globalInitPieces, before calling cs->finishGlobalInits(). basically,
			// it's all handled.
			auto value = magicallyRunExpressionAtCompileTime(state->cs, *stmt, nullptr,
				fir::Name::obfuscate("__anon_runner_", state->fnCounter++),
				state->interpState);

			state->interpState->finalise();

			if(value)
			{
				// if it was an expression, then give it a name so we can refer to it later.
				auto init = util::pool<sst::RawValueExpr>(Location(), value->getType());
				init->rawValue = CGResult(value);

				auto vardef = util::pool<sst::VarDefn>(Location());
				vardef->type = init->type;
				vardef->id = Identifier(zpr::sprint("_%d", state->varCounter++), IdKind::Name);
				vardef->global = true;
				vardef->init = init;

				state->fs->stree->addDefinition(vardef->id.name, vardef);

				zpr::println("%s: %s = %s", vardef->id.name, value->getType(), value->str());
			}
		}


		return false;
	}
}















