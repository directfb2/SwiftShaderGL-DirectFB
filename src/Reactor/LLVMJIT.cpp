// Copyright 2020 The SwiftShader Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "ExecutableMemory.hpp"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Mangler.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "LLVMReactor.hpp"
#ifdef ENABLE_RR_PRINT
#include "Reactor.hpp"
#endif
#include "Routine.hpp"

#include <unordered_map>
#if __has_feature(memory_sanitizer)
#include <sanitizer/msan_interface.h>
#endif

#ifdef __ARM_EABI__
extern "C" signed __aeabi_idivmod();
#endif

namespace {

// JITGlobals is a singleton that holds all the immutable machine specific
// information for the host device.
class JITGlobals
{
public:
	using TargetMachineSPtr = std::shared_ptr<llvm::TargetMachine>;

	static JITGlobals *get();

	const std::string mcpu;
	const std::vector<std::string> mattrs;
	const char *const march;
	const llvm::TargetOptions targetOptions;
	const llvm::DataLayout dataLayout;

	TargetMachineSPtr createTargetMachine(rr::Optimization::Level optlevel);

private:
	using TargetMachineUPtr = std::unique_ptr<llvm::TargetMachine>;

	static JITGlobals create();

	static llvm::CodeGenOpt::Level toLLVM(rr::Optimization::Level level);

	JITGlobals(const char *mcpu,
	           const std::vector<std::string> &mattrs,
	           const char *march,
	           const llvm::TargetOptions &targetOptions,
	           const llvm::DataLayout &dataLayout) :
		mcpu(mcpu),
		mattrs(mattrs),
		march(march),
		targetOptions(targetOptions),
		dataLayout(dataLayout) {}

	JITGlobals(const JITGlobals &) = default;
};

JITGlobals *JITGlobals::get()
{
	static JITGlobals instance = create();
	return &instance;
}

JITGlobals::TargetMachineSPtr JITGlobals::createTargetMachine(rr::Optimization::Level optlevel)
{
#ifdef ENABLE_RR_DEBUG_INFO
	auto llvmOptLevel = toLLVM(rr::Optimization::Level::None);
#else
	auto llvmOptLevel = toLLVM(optlevel);
#endif

	return TargetMachineSPtr(llvm::EngineBuilder().setOptLevel(llvmOptLevel)
	                                              .setMCPU(mcpu)
	                                              .setMArch(march)
	                                              .setMAttrs(mattrs)
	                                              .setTargetOptions(targetOptions)
	                                              .selectTarget());
}

JITGlobals JITGlobals::create()
{
	struct LLVMInitializer
	{
		LLVMInitializer()
		{
			llvm::InitializeNativeTarget();
			llvm::InitializeNativeTargetAsmPrinter();
			llvm::InitializeNativeTargetAsmParser();
		}
	};
	static LLVMInitializer initializeLLVM;

	auto mcpu = llvm::sys::getHostCPUName();

	llvm::StringMap<bool> features;
	bool ok = llvm::sys::getHostCPUFeatures(features);
	ASSERT_MSG(ok, "llvm::sys::getHostCPUFeatures returned false");

	std::vector<std::string> mattrs;
	for(auto &feature : features)
	{
		if(feature.second) { mattrs.push_back(feature.first().str()); }
	}

	const char *march = nullptr;
#if defined(__x86_64__)
	march = "x86-64";
#elif defined(__i386__)
	march = "x86";
#elif defined(__aarch64__)
	march = "arm64";
#elif defined(__arm__)
	march = "arm";
#else
#	error "unknown architecture"
#endif

	llvm::TargetOptions targetOptions;
	targetOptions.UnsafeFPMath = false;

	auto targetMachine = TargetMachineUPtr(llvm::EngineBuilder().setOptLevel(llvm::CodeGenOpt::None)
	                                                            .setMCPU(mcpu)
	                                                            .setMArch(march)
	                                                            .setMAttrs(mattrs)
	                                                            .setTargetOptions(targetOptions)
	                                                            .selectTarget());

	auto dataLayout = targetMachine->createDataLayout();

	return JITGlobals(mcpu.data(), mattrs, march, targetOptions, dataLayout);
}

llvm::CodeGenOpt::Level JITGlobals::toLLVM(rr::Optimization::Level level)
{
	switch(level)
	{
		case rr::Optimization::Level::None:       return ::llvm::CodeGenOpt::None;
		case rr::Optimization::Level::Less:       return ::llvm::CodeGenOpt::Less;
		case rr::Optimization::Level::Default:    return ::llvm::CodeGenOpt::Default;
		case rr::Optimization::Level::Aggressive: return ::llvm::CodeGenOpt::Aggressive;
		default:
			UNREACHABLE("Unknown Optimization Level %d", int(level));
			return ::llvm::CodeGenOpt::Default;
	}
}

class MemoryMapper final : public llvm::SectionMemoryManager::MemoryMapper
{
public:
	MemoryMapper() {}
	~MemoryMapper() final {}

	llvm::sys::MemoryBlock allocateMappedMemory(llvm::SectionMemoryManager::AllocationPurpose purpose,
	                                            size_t numBytes,
	                                            const llvm::sys::MemoryBlock *const nearBlock,
	                                            unsigned flags,
	                                            std::error_code &errorCode) final
	{
		errorCode = std::error_code();

		size_t pageSize = rr::memoryPageSize();
		numBytes = (numBytes + pageSize - 1) & ~(pageSize - 1);

		bool need_exec = purpose == llvm::SectionMemoryManager::AllocationPurpose::Code;
		void *addr = rr::allocateMemoryPages(numBytes, flagsToPermissions(flags), need_exec);
		if(!addr)
			return llvm::sys::MemoryBlock();

		return llvm::sys::MemoryBlock(addr, numBytes);
	}

	std::error_code protectMappedMemory(const llvm::sys::MemoryBlock &block,
	                                    unsigned flags)
	{
		void *addr = block.base();
		size_t size = block.allocatedSize();
		size_t pageSize = rr::memoryPageSize();
		addr = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(addr) & ~(pageSize - 1));
		size += reinterpret_cast<uintptr_t>(block.base()) - reinterpret_cast<uintptr_t>(addr);

		rr::protectMemoryPages(addr, size, flagsToPermissions(flags));

		return std::error_code();
	}

	std::error_code releaseMappedMemory(llvm::sys::MemoryBlock &block)
	{
		size_t size = block.allocatedSize();

		rr::deallocateMemoryPages(block.base(), size);

		return std::error_code();
	}

private:
	int flagsToPermissions(unsigned flags)
	{
		int result = 0;

		if(flags & llvm::sys::Memory::MF_READ)
		{
			result |= rr::PERMISSION_READ;
		}
		if(flags & llvm::sys::Memory::MF_WRITE)
		{
			result |= rr::PERMISSION_WRITE;
		}
		if(flags & llvm::sys::Memory::MF_EXEC)
		{
			result |= rr::PERMISSION_EXECUTE;
		}

		return result;
	}
};

template<typename T>
T alignUp(T val, T alignment)
{
	return alignment * ((val + alignment - 1) / alignment);
}

void *alignedAlloc(size_t size, size_t alignment)
{
	ASSERT(alignment < 256);
	auto allocation = new uint8_t[size + sizeof(uint8_t) + alignment];
	auto aligned = allocation;
	aligned += sizeof(uint8_t);
	aligned = reinterpret_cast<uint8_t *>(alignUp(reinterpret_cast<uintptr_t>(aligned), alignment));
	auto offset = static_cast<uint8_t>(aligned - allocation);
	aligned[-1] = offset;
	return aligned;
}

void alignedFree(void *ptr)
{
	auto aligned = reinterpret_cast<uint8_t *>(ptr);
	auto offset = aligned[-1];
	auto allocation = aligned - offset;
	delete[] allocation;
}

template<typename T>
static void atomicLoad(void *ptr, void *ret, llvm::AtomicOrdering ordering)
{
	*reinterpret_cast<T *>(ret) = std::atomic_load_explicit<T>(reinterpret_cast<std::atomic<T> *>(ptr), rr::atomicOrdering(ordering));
}

template<typename T>
static void atomicStore(void *ptr, void *val, llvm::AtomicOrdering ordering)
{
	std::atomic_store_explicit<T>(reinterpret_cast<std::atomic<T> *>(ptr), *reinterpret_cast<T *>(val), rr::atomicOrdering(ordering));
}

void *resolveExternalSymbol(const char *name)
{
	struct Atomic
	{
		static void load(size_t size, void *ptr, void *ret, llvm::AtomicOrdering ordering)
		{
			switch(size)
			{
				case 1: atomicLoad<uint8_t>(ptr, ret, ordering); break;
				case 2: atomicLoad<uint16_t>(ptr, ret, ordering); break;
				case 4: atomicLoad<uint32_t>(ptr, ret, ordering); break;
				case 8: atomicLoad<uint64_t>(ptr, ret, ordering); break;
				default:
					UNIMPLEMENTED("Atomic::load(size: %d)", int(size));
			}
		}
		static void store(size_t size, void *ptr, void *ret, llvm::AtomicOrdering ordering)
		{
			switch(size)
			{
				case 1: atomicStore<uint8_t>(ptr, ret, ordering); break;
				case 2: atomicStore<uint16_t>(ptr, ret, ordering); break;
				case 4: atomicStore<uint32_t>(ptr, ret, ordering); break;
				case 8: atomicStore<uint64_t>(ptr, ret, ordering); break;
				default:
					UNIMPLEMENTED("Atomic::store(size: %d)", int(size));
			}
		}
	};

	struct F
	{
		static void nop() {}
		static void neverCalled() { UNREACHABLE("Should never be called"); }

		static void *coroutine_alloc_frame(size_t size) { return alignedAlloc(size, 16); }
		static void coroutine_free_frame(void *ptr) { alignedFree(ptr); }
	};

	class Resolver
	{
	public:
		using FunctionMap = std::unordered_map<std::string, void *>;

		FunctionMap functions;

		Resolver()
		{
#ifdef ENABLE_RR_PRINT
			functions.emplace("rr::DebugPrintf", reinterpret_cast<void *>(rr::DebugPrintf));
#endif
			functions.emplace("nop", reinterpret_cast<void *>(F::nop));
			functions.emplace("floorf", reinterpret_cast<void *>(floorf));
			functions.emplace("nearbyintf", reinterpret_cast<void *>(nearbyintf));
			functions.emplace("truncf", reinterpret_cast<void *>(truncf));
			functions.emplace("printf", reinterpret_cast<void *>(printf));
			functions.emplace("puts", reinterpret_cast<void *>(puts));
			functions.emplace("fmodf", reinterpret_cast<void *>(fmodf));

			functions.emplace("sinf", reinterpret_cast<void *>(sinf));
			functions.emplace("cosf", reinterpret_cast<void *>(cosf));
			functions.emplace("asinf", reinterpret_cast<void *>(asinf));
			functions.emplace("acosf", reinterpret_cast<void *>(acosf));
			functions.emplace("atanf", reinterpret_cast<void *>(atanf));
			functions.emplace("sinhf", reinterpret_cast<void *>(sinhf));
			functions.emplace("coshf", reinterpret_cast<void *>(coshf));
			functions.emplace("tanhf", reinterpret_cast<void *>(tanhf));
			functions.emplace("asinhf", reinterpret_cast<void *>(asinhf));
			functions.emplace("acoshf", reinterpret_cast<void *>(acoshf));
			functions.emplace("atanhf", reinterpret_cast<void *>(atanhf));
			functions.emplace("atan2f", reinterpret_cast<void *>(atan2f));
			functions.emplace("powf", reinterpret_cast<void *>(powf));
			functions.emplace("expf", reinterpret_cast<void *>(expf));
			functions.emplace("logf", reinterpret_cast<void *>(logf));
			functions.emplace("exp2f", reinterpret_cast<void *>(exp2f));
			functions.emplace("log2f", reinterpret_cast<void *>(log2f));

			functions.emplace("sin", reinterpret_cast<void *>(static_cast<double (*)(double)>(sin)));
			functions.emplace("cos", reinterpret_cast<void *>(static_cast<double (*)(double)>(cos)));
			functions.emplace("asin", reinterpret_cast<void *>(static_cast<double (*)(double)>(asin)));
			functions.emplace("acos", reinterpret_cast<void *>(static_cast<double (*)(double)>(acos)));
			functions.emplace("atan", reinterpret_cast<void *>(static_cast<double (*)(double)>(atan)));
			functions.emplace("sinh", reinterpret_cast<void *>(static_cast<double (*)(double)>(sinh)));
			functions.emplace("cosh", reinterpret_cast<void *>(static_cast<double (*)(double)>(cosh)));
			functions.emplace("tanh", reinterpret_cast<void *>(static_cast<double (*)(double)>(tanh)));
			functions.emplace("asinh", reinterpret_cast<void *>(static_cast<double (*)(double)>(asinh)));
			functions.emplace("acosh", reinterpret_cast<void *>(static_cast<double (*)(double)>(acosh)));
			functions.emplace("atanh", reinterpret_cast<void *>(static_cast<double (*)(double)>(atanh)));
			functions.emplace("atan2", reinterpret_cast<void *>(static_cast<double (*)(double, double)>(atan2)));
			functions.emplace("pow", reinterpret_cast<void *>(static_cast<double (*)(double, double)>(pow)));
			functions.emplace("exp", reinterpret_cast<void *>(static_cast<double (*)(double)>(exp)));
			functions.emplace("log", reinterpret_cast<void *>(static_cast<double (*)(double)>(log)));
			functions.emplace("exp2", reinterpret_cast<void *>(static_cast<double (*)(double)>(exp2)));
			functions.emplace("log2", reinterpret_cast<void *>(static_cast<double (*)(double)>(log2)));

			functions.emplace("atomic_load", reinterpret_cast<void *>(Atomic::load));
			functions.emplace("atomic_store", reinterpret_cast<void *>(Atomic::store));

			functions.emplace("coroutine_alloc_frame", reinterpret_cast<void *>(F::coroutine_alloc_frame));
			functions.emplace("coroutine_free_frame", reinterpret_cast<void *>(F::coroutine_free_frame));

			functions.emplace("sincosf", reinterpret_cast<void *>(sincosf));

#ifdef __ARM_EABI__
			functions.emplace("aeabi_idivmod", reinterpret_cast<void *>(__aeabi_idivmod));
#endif

#if __has_feature(memory_sanitizer)
			functions.emplace("msan_unpoison", reinterpret_cast<void *>(__msan_unpoison));
#endif
		}
	};

	static Resolver resolver;

	// Trim off any underscores from the start of the symbol.
	const char *trimmed = name;
	while(trimmed[0] == '_') { trimmed++; }

	auto it = resolver.functions.find(trimmed);
	ASSERT_MSG(it != resolver.functions.end(), "Missing external function: '%s'", name);
	return it->second;
}

class JITRoutine : public rr::Routine
{
	using ObjLayer = llvm::orc::LegacyRTDyldObjectLinkingLayer;
	using CompileLayer = llvm::orc::LegacyIRCompileLayer<ObjLayer, llvm::orc::SimpleCompiler>;

public:
#if defined(__clang__)
#	pragma clang diagnostic push
#	pragma clang diagnostic ignored "-Wdeprecated-declarations"
#elif defined(__GNUC__)
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

	JITRoutine(std::unique_ptr<llvm::Module> module,
	           llvm::Function **funcs,
	           size_t count,
	           const rr::Config &config) :
		resolver(createLegacyLookupResolver(session,
		                                    [&](const llvm::StringRef &name)
		                                    {
		                                      void *func = resolveExternalSymbol(name.str().c_str());
		                                      if(func != nullptr)
		                                      {
		                                   	    return llvm::JITSymbol(reinterpret_cast<uintptr_t>(func), llvm::JITSymbolFlags::Absolute);
		                                      }
		                                      return objLayer.findSymbol(name, true);
		                                    },
		                                    [](llvm::Error err) { if(err) { return; } })),
		targetMachine(JITGlobals::get()->createTargetMachine(config.getOptimization().getLevel())),
		compileLayer(objLayer, llvm::orc::SimpleCompiler(*targetMachine)),
		objLayer(session,
		         [this](llvm::orc::VModuleKey)
		         {
		           return ObjLayer::Resources{ std::make_shared<llvm::SectionMemoryManager>(&memoryMapper), resolver };
		         },
		         ObjLayer::NotifyLoadedFtor(),
		         [](llvm::orc::VModuleKey, const llvm::object::ObjectFile &Obj, const llvm::RuntimeDyld::LoadedObjectInfo &L)
		         {
#ifdef ENABLE_RR_DEBUG_INFO
		           rr::DebugInfo::NotifyObjectEmitted(Obj, L);
#endif
		         },
		         [](llvm::orc::VModuleKey, const llvm::object::ObjectFile &Obj)
		         {
#ifdef ENABLE_RR_DEBUG_INFO
		           rr::DebugInfo::NotifyFreeingObject(Obj);
#endif
		         }),
		addresses(count)
	{
#if defined(__clang__)
#	pragma clang diagnostic pop
#elif defined(__GNUC__)
#	pragma GCC diagnostic pop
#endif

		std::vector<std::string> mangledNames(count);
		for(size_t i = 0; i < count; i++)
		{
			auto func = funcs[i];
			static std::atomic<size_t> numEmittedFunctions = { 0 };
			std::string name = "f" + llvm::Twine(numEmittedFunctions++).str();
			func->setName(name);
			func->setLinkage(llvm::GlobalValue::ExternalLinkage);
			func->setDoesNotThrow();

			llvm::raw_string_ostream mangledNameStream(mangledNames[i]);
			llvm::Mangler::getNameWithPrefix(mangledNameStream, name, JITGlobals::get()->dataLayout);
		}

		auto moduleKey = session.allocateVModule();

		llvm::cantFail(compileLayer.addModule(moduleKey, std::move(module)));

		// Resolve the function addresses.
		for(size_t i = 0; i < count; i++)
		{
			auto symbol = compileLayer.findSymbolIn(moduleKey, mangledNames[i], false);
			if(auto address = symbol.getAddress())
			{
				addresses[i] = reinterpret_cast<void *>(static_cast<intptr_t>(address.get()));
			}
		}
	}

	const void *getEntry(int index) const override
	{
		return addresses[index];
	}

private:
	std::shared_ptr<llvm::orc::SymbolResolver> resolver;
	std::shared_ptr<llvm::TargetMachine> targetMachine;
	llvm::orc::ExecutionSession session;
	CompileLayer compileLayer;
	MemoryMapper memoryMapper;
	ObjLayer objLayer;
	std::vector<const void *> addresses;
};

}

namespace rr {

JITBuilder::JITBuilder(const Config &config) :
	config(config),
	module(new llvm::Module("", context)),
	builder(new llvm::IRBuilder<>(context))
{
	module->setDataLayout(JITGlobals::get()->dataLayout);
}

void JITBuilder::optimize(const Config &cfg)
{
#ifdef ENABLE_RR_DEBUG_INFO
	if(debugInfo != nullptr)
	{
		return; // Don't optimize if we're generating debug info.
	}
#endif

	std::unique_ptr<llvm::legacy::PassManager> passManager(new llvm::legacy::PassManager());

	for(auto pass : cfg.getOptimization().getPasses())
	{
		switch(pass)
		{
			case Optimization::Pass::Disabled: break;
			case Optimization::Pass::CFGSimplification: passManager->add(llvm::createCFGSimplificationPass()); break;
			case Optimization::Pass::LICM: passManager->add(llvm::createLICMPass()); break;
			case Optimization::Pass::AggressiveDCE: passManager->add(llvm::createAggressiveDCEPass()); break;
			case Optimization::Pass::GVN: passManager->add(llvm::createGVNPass()); break;
			case Optimization::Pass::InstructionCombining: passManager->add(llvm::createInstructionCombiningPass()); break;
			case Optimization::Pass::Reassociate: passManager->add(llvm::createReassociatePass()); break;
			case Optimization::Pass::DeadStoreElimination: passManager->add(llvm::createDeadStoreEliminationPass()); break;
			case Optimization::Pass::SCCP: passManager->add(llvm::createSCCPPass()); break;
			case Optimization::Pass::ScalarReplAggregates: passManager->add(llvm::createSROAPass()); break;
			case Optimization::Pass::EarlyCSEPass: passManager->add(llvm::createEarlyCSEPass()); break;
			default:
				UNREACHABLE("pass: %d", int(pass));
				break;
		}
	}

	passManager->run(*module);
}

std::shared_ptr<Routine> JITBuilder::acquireRoutine(llvm::Function **funcs, size_t count, const Config &cfg)
{
	ASSERT(module);
	return std::make_shared<JITRoutine>(std::move(module), funcs, count, cfg);
}

}
