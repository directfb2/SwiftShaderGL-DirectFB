// Copyright 2019 The SwiftShader Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef rr_LLVMReactor_hpp
#define rr_LLVMReactor_hpp

#include "Debug.hpp"
#include "llvm/IR/IRBuilder.h"
#ifdef ENABLE_RR_DEBUG_INFO
#include "LLVMReactorDebugInfo.hpp"
#endif
#include "Nucleus.hpp"

namespace rr {

llvm::Type *T(Type *t);

inline Type *T(llvm::Type *t)
{
	return reinterpret_cast<Type *>(t);
}

inline llvm::Value *V(Value *t)
{
	return reinterpret_cast<llvm::Value *>(t);
}

inline Value *V(llvm::Value *t)
{
	return reinterpret_cast<Value *>(t);
}

inline std::vector<llvm::Value *> V(const std::vector<Value *> &values)
{
	std::vector<llvm::Value *> result;
	result.reserve(values.size());
	for(auto &v : values)
	{
		result.push_back(V(v));
	}
	return result;
}

// Emits a no-op instruction that will not be optimized away.
// Useful for emitting something that can have a source location without effect.
void Nop();

// JITBuilder holds all the LLVM state for building routines.
class JITBuilder
{
public:
	JITBuilder(const Config &config);

	void optimize(const Config &cfg);

	std::shared_ptr<Routine> acquireRoutine(llvm::Function **funcs, size_t count, const Config &cfg);

	const Config config;
	llvm::LLVMContext context;
	std::unique_ptr<llvm::Module> module;
	std::unique_ptr<llvm::IRBuilder<>> builder;
	llvm::Function *function = nullptr;

#ifdef ENABLE_RR_DEBUG_INFO
	std::unique_ptr<DebugInfo> debugInfo;
#endif
};

inline std::memory_order atomicOrdering(llvm::AtomicOrdering memoryOrder)
{
	switch(memoryOrder)
	{
		case llvm::AtomicOrdering::Monotonic:              return std::memory_order_relaxed;
		case llvm::AtomicOrdering::Acquire:                return std::memory_order_acquire;
		case llvm::AtomicOrdering::Release:                return std::memory_order_release;
		case llvm::AtomicOrdering::AcquireRelease:         return std::memory_order_acq_rel;
		case llvm::AtomicOrdering::SequentiallyConsistent: return std::memory_order_seq_cst;
		default:
			UNREACHABLE("memoryOrder: %d", int(memoryOrder));
			return std::memory_order_acq_rel;
	}
}

inline llvm::AtomicOrdering atomicOrdering(bool atomic, std::memory_order memoryOrder)
{
	if(!atomic)
	{
		return llvm::AtomicOrdering::NotAtomic;
	}

	switch(memoryOrder)
	{
		case std::memory_order_relaxed: return llvm::AtomicOrdering::Monotonic;
		case std::memory_order_consume: return llvm::AtomicOrdering::Acquire;
		case std::memory_order_acquire: return llvm::AtomicOrdering::Acquire;
		case std::memory_order_release: return llvm::AtomicOrdering::Release;
		case std::memory_order_acq_rel: return llvm::AtomicOrdering::AcquireRelease;
		case std::memory_order_seq_cst: return llvm::AtomicOrdering::SequentiallyConsistent;
		default:
			UNREACHABLE("memoryOrder: %d", int(memoryOrder));
			return llvm::AtomicOrdering::AcquireRelease;
	}
}

}

#endif
