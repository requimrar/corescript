// jit.cpp
// Copyright (c) 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "errors.h"
#include "backends/llvm.h"

namespace backend
{
	LLVMJit::LLVMJit(llvm::TargetMachine* tm) :
		targetMachine(tm),
		symbolResolver(llvm::orc::createLegacyLookupResolver(this->execSession, [&](const std::string& name) -> llvm::JITSymbol {
			if(auto sym = this->compileLayer.findSymbol(name, false))   return sym;
			else if(auto err = sym.takeError())                         return std::move(err);

			if(auto symaddr = llvm::RTDyldMemoryManager::getSymbolAddressInProcess(name))
				return llvm::JITSymbol(symaddr, llvm::JITSymbolFlags::Exported);
			else
				return llvm::JITSymbol(nullptr);
		}, [](llvm::Error err) { llvm::cantFail(std::move(err), "lookupFlags failed"); })),
		dataLayout(this->targetMachine->createDataLayout()),
		objectLayer(this->execSession, [this](llvm::orc::VModuleKey) -> auto {
			return llvm::orc::RTDyldObjectLinkingLayer::Resources {
				std::make_shared<llvm::SectionMemoryManager>(), this->symbolResolver }; }),
		compileLayer(this->objectLayer, llvm::orc::SimpleCompiler(*this->targetMachine.get()))
	{
		llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);
	}

	llvm::TargetMachine* LLVMJit::getTargetMachine()
	{
		return this->targetMachine.get();
	}

	LLVMJit::ModuleHandle_t LLVMJit::addModule(std::unique_ptr<llvm::Module> mod)
	{
		auto vmod = this->execSession.allocateVModule();
		llvm::cantFail(this->compileLayer.addModule(vmod, std::move(mod)));

		return vmod;
	}

	void LLVMJit::removeModule(LLVMJit::ModuleHandle_t mod)
	{
		llvm::cantFail(this->compileLayer.removeModule(mod));
	}

	llvm::JITSymbol LLVMJit::findSymbol(const std::string& name)
	{
		std::string mangledName;
		llvm::raw_string_ostream out(mangledName);
		llvm::Mangler::getNameWithPrefix(out, name, this->dataLayout);

		return this->compileLayer.findSymbol(out.str(), false);
	}

	llvm::JITTargetAddress LLVMJit::getSymbolAddress(const std::string& name)
	{
		auto addr = this->findSymbol(name).getAddress();
		if(!addr)
		{
			std::string err;
			auto out = llvm::raw_string_ostream(err);

			out << addr.takeError();
			error("llvm: failed to find symbol '%s' (%s)", name, out.str());
		}

		return addr.get();
	}
}




















