// Compiler.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <iostream>
#include <fstream>
#include <cassert>
#include <fstream>
#include <cstdlib>
#include <cinttypes>

#include <sys/stat.h>
#include "../include/parser.h"
#include "../include/codegen.h"
#include "../include/compiler.h"
#include "../include/dependency.h"

#include "llvm/IR/Verifier.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Bitcode/ReaderWriter.h"

using namespace Ast;

namespace Compiler
{
	std::string resolveImport(Import* imp, std::string fullPath)
	{
		std::string curpath = getPathFromFile(fullPath);

		if(imp->module.find("*") != (size_t) -1)
		{
			Parser::parserError("Wildcard imports are currently not supported (trying to import %s)", imp->module.c_str());
		}

		// first check the current directory.
		std::string modname = imp->module;
		for(size_t i = 0; i < modname.length(); i++)
		{
			if(modname[i] == '.')
				modname[i] = '/';
		}

		std::string name = curpath + "/" + modname + ".flx";
		char* fname = realpath(name.c_str(), 0);

		// a file here
		if(fname != NULL)
		{
			auto ret = std::string(fname);
			free(fname);
			return getFullPathOfFile(ret);
		}
		else
		{
			free(fname);
			std::string builtinlib = getSysroot() + getPrefix() + modname + ".flx";

			struct stat buffer;
			if(stat(builtinlib.c_str(), &buffer) == 0)
			{
				return getFullPathOfFile(builtinlib);
			}
			else
			{
				std::string msg = "No module or library with the name '" + modname + "' could be found (no such builtin library either)";

				va_list ap;
				__error_gen(getFileLines(fullPath), imp->posinfo.line + 1 /* idk why */, imp->posinfo.col,
					getFilenameFromPath(fullPath).c_str(), msg.c_str(), "Error", true, ap);

				abort();
			}
		}
	}




	// static void addSubs(Codegen::CodegenInstance* cgi, Codegen::FunctionTree* rootft, Codegen::FunctionTree* sub)
	// {
	// 	for(auto s : rootft->subs)
	// 	{
	// 		if(s->nsName == sub->nsName)
	// 		{
	// 			// printf("found %s, not cloning (has %zu subs)\n", s->nsName.c_str(), sub->subs.size());
	// 			// add subs of subs instead.
	// 			for(auto ss : sub->subs)
	// 				addSubs(cgi, s, ss);
	// 		}
	// 	}

	// 	rootft->subs.push_back(cgi->cloneFunctionTree(sub, false));
	// };


	static void cloneCGIInnards(Codegen::CodegenInstance* from, Codegen::CodegenInstance* to)
	{
		to->typeMap					= from->typeMap;
		to->customOperatorMap		= from->customOperatorMap;
		to->customOperatorMapRev	= from->customOperatorMapRev;
		to->globalConstructors		= from->globalConstructors;
	}

	static void copyRootInnards(Codegen::CodegenInstance* cgi, Root* from, Root* to, bool doClone)
	{
		using namespace Codegen;
		// for(auto f : from->publicFuncTree.funcs)
		// {
		// 	printf("*** copying public func %s (%d -> %d)\n", f.first->getName().bytes_begin(), from->publicFuncTree.id,
		// 		to->publicFuncTree.id);

		// 	// to->.funcs.push_back(f);
		// 	to->publicFuncTree.funcs.push_back(f);
		// }


		// for(auto s : from->publicFuncTree.subs)
		// {
		// 	// addSubs(&to->externalFuncTree, s);
		// 	addSubs(cgi, &to->publicFuncTree, s);
		// }

		for(auto v : from->publicTypes)
		{
			to->externalTypes.push_back(std::pair<StructBase*, llvm::Type*>(v.first, v.second));
			to->publicTypes.push_back(std::pair<StructBase*, llvm::Type*>(v.first, v.second));
		}

		for(auto v : from->publicGenericFunctions)
		{
			to->externalGenericFunctions.push_back(v);
			to->publicGenericFunctions.push_back(v);
		}

		for(auto v : from->typeList)
		{
			bool skip = false;
			for(auto k : to->typeList)
			{
				if(std::get<0>(k) == std::get<0>(v))
				{
					skip = true;
					break;
				}
			}

			if(skip)
				continue;

			to->typeList.push_back(v);
		}

		if(doClone)
		{
			cgi->cloneFunctionTree(from->rootFuncStack, to->rootFuncStack, false);
			cgi->cloneFunctionTree(from->publicFuncTree, to->publicFuncTree, false);
		}


		for(auto v : from->rootFuncStack->vars)
			to->rootFuncStack->vars[v.first] = v.second;
	}

	static std::pair<Codegen::CodegenInstance*, std::string> _compileFile(std::string fpath, Codegen::CodegenInstance* rcgi, Root* dummyRoot)
	{
		using namespace Codegen;
		using namespace Parser;

		CodegenInstance* cgi = new CodegenInstance();
		cloneCGIInnards(rcgi, cgi);

		cgi->rawLines = Compiler::getFileLines(fpath);
		ParserState pstate(cgi);

		cgi->customOperatorMap = rcgi->customOperatorMap;
		cgi->customOperatorMapRev = rcgi->customOperatorMapRev;

		std::string curpath = Compiler::getPathFromFile(fpath);
		pstate.cgi->rawLines = Compiler::getFileLines(fpath);

		// parse
		printf("\n\n** COMPILING: %s\n\n\n", Compiler::getFilenameFromPath(fpath).c_str());
		Root* root = Parser::Parse(pstate, fpath);
		cgi->rootNode = root;

		// add the previous stuff to our own root
		copyRootInnards(cgi, dummyRoot, root, true);


		Codegen::doCodegen(fpath, root, cgi);

		llvm::verifyModule(*cgi->module, &llvm::errs());
		Codegen::writeBitcode(fpath, pstate.cgi);

		size_t lastdot = fpath.find_last_of(".");
		std::string oname = (lastdot == std::string::npos ? fpath : fpath.substr(0, lastdot));
		oname += ".bc";


		printf("\n\n** COPYING BACK\n\n\n");


		// add the new stuff to the main root
		// todo: check for duplicates
		copyRootInnards(rcgi, root, dummyRoot, true);

		printf("\n\n** DONE WITH: %s\n\n\n", Compiler::getFilenameFromPath(fpath).c_str());

		rcgi->customOperatorMap = cgi->customOperatorMap;
		rcgi->customOperatorMapRev = cgi->customOperatorMapRev;

		return { cgi, oname };
	}


	static void _resolveImportGraph(Codegen::DependencyGraph* g, std::unordered_map<std::string, bool>& visited, std::string currentMod,
		std::string curpath)
	{
		using namespace Parser;

		// NOTE: make sure resolveImport **DOES NOT** use codegeninstance, cuz it's 0.
		ParserState fakeps(0);
		fakeps.currentPos.file = currentMod;
		fakeps.currentPos.line = 1;
		fakeps.currentPos.col = 1;

		fakeps.tokens = Compiler::getFileTokens(currentMod);

		while(fakeps.tokens.size() > 0)
		{
			Token t = fakeps.front();
			fakeps.pop_front();

			if(t.type == TType::Import)
			{
				// hack: parseImport expects front token to be "import"
				fakeps.tokens.push_front(t);

				Import* imp = parseImport(fakeps);

				std::string file = Compiler::getFullPathOfFile(Compiler::resolveImport(imp, Compiler::getFullPathOfFile(currentMod)));

				g->addModuleDependency(currentMod, file, imp);

				if(!visited[file])
				{
					visited[file] = true;
					_resolveImportGraph(g, visited, file, curpath);
				}
			}
		}
	}

	static Codegen::DependencyGraph* resolveImportGraph(std::string baseFullPath, std::string curpath)
	{
		using namespace Codegen;
		DependencyGraph* g = new DependencyGraph();

		std::unordered_map<std::string, bool> visited;
		_resolveImportGraph(g, visited, baseFullPath, curpath);

		return g;
	}




	std::tuple<Root*, std::vector<std::string>, std::unordered_map<std::string, Root*>, std::vector<llvm::Module*>>
	compileFile(std::string filename)
	{
		using namespace Codegen;

		filename = getFullPathOfFile(filename);
		std::string curpath = getPathFromFile(filename);

		DependencyGraph* g = resolveImportGraph(filename, curpath);

		std::deque<std::deque<DepNode*>> groups = g->findCyclicDependencies();

		for(auto gr : groups)
		{
			if(gr.size() > 1)
			{
				std::string modlist;
				std::deque<Expr*> imps;

				for(auto m : gr)
				{
					std::string fn = getFilenameFromPath(m->name);
					fn = fn.substr(0, fn.find_last_of('.'));

					modlist += "\t" + fn + "\n";
				}

				info("Cyclic import dependencies between these modules:\n%s", modlist.c_str());
				info("Offending import statements:");

				for(auto m : gr)
				{
					for(auto u : m->users)
					{
						va_list ap;

						__error_gen(getFileLines(u.first->name), u.second->posinfo.line + 1 /* idk why */, u.second->posinfo.col,
							getFilenameFromPath(u.first->name).c_str(), "", "Note", false, ap);
					}
				}

				error("Cyclic dependencies found, cannot continue");
			}
		}

		std::vector<std::string> outlist;
		std::unordered_map<std::string, Root*> rootmap;
		std::vector<llvm::Module*> modules;


		Root* dummyRoot = new Root();
		CodegenInstance* rcgi = new CodegenInstance();

		for(auto gr : groups)
		{
			iceAssert(gr.size() == 1);
			std::string name = Compiler::getFullPathOfFile(gr.front()->name);

			auto pair = _compileFile(name, rcgi, dummyRoot);
			CodegenInstance* cgi = pair.first;

			modules.push_back(cgi->module);
			outlist.push_back(pair.second);
			rootmap[name] = cgi->rootNode;

			delete cgi;
		}

		return std::make_tuple(rootmap[Compiler::getFullPathOfFile(filename)], outlist, rootmap, modules);
	}
















































	void compileProgram(Codegen::CodegenInstance* cgi, std::vector<std::string> filelist, std::string foldername, std::string outname)
	{
		std::string tgt;
		if(!getTarget().empty())
			tgt = "-target " + getTarget();


		if(!Compiler::getIsCompileOnly() && !cgi->module->getFunction("main"))
		{
			error(0, "No main() function, a program cannot be compiled.");
		}



		std::string oname = outname.empty() ? (foldername + "/" + cgi->module->getModuleIdentifier()).c_str() : outname.c_str();
		// compile it by invoking clang on the bitcode
		char* inv = new char[1024];
		snprintf(inv, 1024, "llvm-link -o '%s.bc'", oname.c_str());
		std::string llvmlink = inv;
		for(auto s : filelist)
			llvmlink += " '" + s + "'";

		system(llvmlink.c_str());

		memset(inv, 0, 1024);
		{
			int opt = Compiler::getOptimisationLevel();
			const char* optLevel	= (Compiler::getOptimisationLevel() >= 0 ? ("-O" + std::to_string(opt)) : "").c_str();
			const char* mcmodel		= (getMcModel().empty() ? "" : ("-mcmodel=" + getMcModel())).c_str();
			const char* isPic		= (getIsPositionIndependent() ? "-fPIC" : "");
			const char* target		= (tgt).c_str();
			const char* outputMode	= (Compiler::getIsCompileOnly() ? "-c" : "");

			snprintf(inv, 1024, "clang++ -flto %s %s %s %s %s -o '%s' '%s.bc'", optLevel, mcmodel, target, isPic, outputMode, oname.c_str(), oname.c_str());
		}
		std::string final = inv;

		// todo: clang bug, http://clang.llvm.org/doxygen/CodeGenAction_8cpp_source.html:714
		// that warning is not affected by any flags I can pass
		// besides, LLVM itself should have caught everything.

		if(!Compiler::getPrintClangOutput())
			final += " &>/dev/null";

		system(final.c_str());

		remove((oname + ".bc").c_str());
		delete[] inv;
	}




	std::string getPathFromFile(std::string path)
	{
		std::string ret;

		size_t sep = path.find_last_of("\\/");
		if(sep != std::string::npos)
			ret = path.substr(0, sep);

		return ret;
	}

	std::string getFilenameFromPath(std::string path)
	{
		std::string ret;

		size_t sep = path.find_last_of("\\/");
		if(sep != std::string::npos)
			ret = path.substr(sep + 1);

		return ret;
	}

	std::string getFullPathOfFile(std::string partial)
	{
		const char* fullpath = realpath(partial.c_str(), 0);
		iceAssert(fullpath);

		std::string ret = fullpath;
		free((void*) fullpath);

		return ret;
	}
}





















