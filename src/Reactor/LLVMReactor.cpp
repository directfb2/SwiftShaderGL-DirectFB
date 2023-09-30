// Copyright 2016 The SwiftShader Authors. All Rights Reserved.
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

#include "LLVMReactor.hpp"

#include "llvm/IR/IntrinsicsX86.h"
#ifdef ENABLE_RR_LLVM_IR_VERIFICATION
#include "llvm/IR/LegacyPassManager.h"
#endif
#include "llvm/IR/Verifier.h"
#include "Reactor.hpp"
#if defined(__i386__) || defined(__x86_64__)
#include "x86.hpp"
#endif

#include <numeric>
#if defined(__i386__) || defined(__x86_64__)
#include <xmmintrin.h>
#endif

#if !LLVM_ENABLE_THREADS
#	error "LLVM_ENABLE_THREADS needs to be enabled"
#endif

namespace {

bool checkSSE41()
{
	int registers[4];
	#if defined(__i386__) || defined(__x86_64__)
	__asm volatile("cpuid": "=a" (registers[0]), "=b" (registers[1]), "=c" (registers[2]), "=d" (registers[3]): "a" (1));
	#else
	registers[0] = registers[1] = registers[2] = registers[3] = 0;
	#endif
	return (registers[2] & 0x00080000) != 0;
}

bool hasSSE41 = checkSSE41();

thread_local rr::JITBuilder *jit = nullptr;

// Default configuration settings. Must be accessed under mutex lock.
std::mutex defaultConfigLock;
rr::Config &defaultConfig()
{
	// This uses a static in a function to avoid the cost of a global static initializer.
	static rr::Config config = rr::Config::Edit().add(rr::Optimization::Pass::ScalarReplAggregates)
	                                             .add(rr::Optimization::Pass::InstructionCombining)
	                                             .apply({});

	return config;
}

llvm::Value *lowerPMINMAX(llvm::Value *x, llvm::Value *y, llvm::ICmpInst::Predicate pred)
{
	return jit->builder->CreateSelect(jit->builder->CreateICmp(pred, x, y), x, y);
}

llvm::Value *lowerPCMP(llvm::ICmpInst::Predicate pred, llvm::Value *x, llvm::Value *y, llvm::Type *dstTy)
{
	return jit->builder->CreateSExt(jit->builder->CreateICmp(pred, x, y), dstTy, "");
}

#if defined(__i386__) || defined(__x86_64__)
llvm::Value *lowerPMOV(llvm::Value *op, llvm::Type *dstType, bool sext)
{
	llvm::VectorType *srcTy = llvm::cast<llvm::VectorType>(op->getType());
	llvm::VectorType *dstTy = llvm::cast<llvm::VectorType>(dstType);

	llvm::Value *undef = llvm::UndefValue::get(srcTy);
	llvm::SmallVector<uint32_t, 16> mask(dstTy->getNumElements());
	std::iota(mask.begin(), mask.end(), 0);

	llvm::Value *v = jit->builder->CreateShuffleVector(op, undef, mask);

	return sext ? jit->builder->CreateSExt(v, dstTy) : jit->builder->CreateZExt(v, dstTy);
}
#endif

#if !defined(__i386__) && !defined(__x86_64__)
llvm::Value *lowerPFMINMAX(llvm::Value *x, llvm::Value *y, llvm::FCmpInst::Predicate pred)
{
	return jit->builder->CreateSelect(jit->builder->CreateFCmp(pred, x, y), x, y);
}

llvm::Value *lowerRound(llvm::Value *x)
{
	llvm::Function *nearbyint = llvm::Intrinsic::getDeclaration(jit->module.get(), llvm::Intrinsic::nearbyint, { x->getType() });

	return jit->builder->CreateCall(nearbyint, { x });
}

llvm::Value *lowerRoundInt(llvm::Value *x, llvm::Type *ty)
{
	return jit->builder->CreateFPToSI(lowerRound(x), ty);
}

llvm::Value *lowerFloor(llvm::Value *x)
{
	llvm::Function *floor = llvm::Intrinsic::getDeclaration(jit->module.get(), llvm::Intrinsic::floor, { x->getType() });

	return jit->builder->CreateCall(floor, { x });
}

llvm::Value *lowerTrunc(llvm::Value *x)
{
	llvm::Function *trunc = llvm::Intrinsic::getDeclaration(jit->module.get(), llvm::Intrinsic::trunc, { x->getType() });

	return jit->builder->CreateCall(trunc, { x });
}

llvm::Value *lowerSQRT(llvm::Value *x)
{
	llvm::Function *sqrt = llvm::Intrinsic::getDeclaration(jit->module.get(), llvm::Intrinsic::sqrt, { x->getType() });

	return jit->builder->CreateCall(sqrt, { x });
}

llvm::Value *lowerRCP(llvm::Value *x)
{
	llvm::Type *ty = x->getType();
	llvm::Constant *one;

	if(llvm::VectorType *vectorTy = llvm::dyn_cast<llvm::VectorType>(ty))
	{
		one = llvm::ConstantVector::getSplat(vectorTy->getNumElements(), llvm::ConstantFP::get(vectorTy->getElementType(), 1));
	}
	else
	{
		one = llvm::ConstantFP::get(ty, 1);
	}

	return jit->builder->CreateFDiv(one, x);
}

llvm::Value *lowerRSQRT(llvm::Value *x)
{
	return lowerRCP(lowerSQRT(x));
}

llvm::Value *lowerVectorShl(llvm::Value *x, uint64_t scalarY)
{
	llvm::VectorType *ty = llvm::cast<llvm::VectorType>(x->getType());
	llvm::Value *y = llvm::ConstantVector::getSplat(ty->getNumElements(), llvm::ConstantInt::get(ty->getElementType(), scalarY));

	return jit->builder->CreateShl(x, y);
}

llvm::Value *lowerVectorAShr(llvm::Value *x, uint64_t scalarY)
{
	llvm::VectorType *ty = llvm::cast<llvm::VectorType>(x->getType());
	llvm::Value *y = llvm::ConstantVector::getSplat(ty->getNumElements(), llvm::ConstantInt::get(ty->getElementType(), scalarY));

	return jit->builder->CreateAShr(x, y);
}

llvm::Value *lowerVectorLShr(llvm::Value *x, uint64_t scalarY)
{
	llvm::VectorType *ty = llvm::cast<llvm::VectorType>(x->getType());
	llvm::Value *y = llvm::ConstantVector::getSplat(ty->getNumElements(), llvm::ConstantInt::get(ty->getElementType(), scalarY));

	return jit->builder->CreateLShr(x, y);
}

llvm::Value *lowerMulAdd(llvm::Value *x, llvm::Value *y)
{
	llvm::VectorType *ty = llvm::cast<llvm::VectorType>(x->getType());
	llvm::VectorType *extTy = llvm::VectorType::getExtendedElementVectorType(ty);

	llvm::Value *extX = jit->builder->CreateSExt(x, extTy);
	llvm::Value *extY = jit->builder->CreateSExt(y, extTy);
	llvm::Value *mult = jit->builder->CreateMul(extX, extY);

	llvm::Value *undef = llvm::UndefValue::get(extTy);

	llvm::SmallVector<uint32_t, 16> evenIdx;
	llvm::SmallVector<uint32_t, 16> oddIdx;

	for(uint64_t i = 0, n = ty->getNumElements(); i < n; i += 2)
	{
		evenIdx.push_back(i);
		oddIdx.push_back(i + 1);
	}

	llvm::Value *lhs = jit->builder->CreateShuffleVector(mult, undef, evenIdx);
	llvm::Value *rhs = jit->builder->CreateShuffleVector(mult, undef, oddIdx);

	return jit->builder->CreateAdd(lhs, rhs);
}

llvm::Value *lowerPack(llvm::Value *x, llvm::Value *y, bool isSigned)
{
	llvm::VectorType *srcTy = llvm::cast<llvm::VectorType>(x->getType());
	llvm::VectorType *dstTy = llvm::VectorType::getTruncatedElementVectorType(srcTy);

	llvm::IntegerType *dstElemTy = llvm::cast<llvm::IntegerType>(dstTy->getElementType());

	uint64_t truncNumBits = dstElemTy->getIntegerBitWidth();
	ASSERT_MSG(truncNumBits < 64, "shift 64 must be handled separately => truncNumBits: %d", int(truncNumBits));

	llvm::Constant *max, *min;
	if(isSigned)
	{
		max = llvm::ConstantInt::get(srcTy, (1LL << (truncNumBits - 1)) - 1, true);
		min = llvm::ConstantInt::get(srcTy, (-1LL << (truncNumBits - 1)), true);
	}
	else
	{
		max = llvm::ConstantInt::get(srcTy, (1ULL << truncNumBits) - 1, false);
		min = llvm::ConstantInt::get(srcTy, 0, false);
	}

	x = lowerPMINMAX(x, min, llvm::ICmpInst::ICMP_SGT);
	x = lowerPMINMAX(x, max, llvm::ICmpInst::ICMP_SLT);
	y = lowerPMINMAX(y, min, llvm::ICmpInst::ICMP_SGT);
	y = lowerPMINMAX(y, max, llvm::ICmpInst::ICMP_SLT);

	x = jit->builder->CreateTrunc(x, dstTy);
	y = jit->builder->CreateTrunc(y, dstTy);

	llvm::SmallVector<uint32_t, 16> index(srcTy->getNumElements() * 2);
	std::iota(index.begin(), index.end(), 0);

	return jit->builder->CreateShuffleVector(x, y, index);
}

llvm::Value *lowerSignMask(llvm::Value *x, llvm::Type *retTy)
{
	llvm::VectorType *ty = llvm::cast<llvm::VectorType>(x->getType());
	llvm::Constant *zero = llvm::ConstantInt::get(ty, 0);
	llvm::Value *cmp = jit->builder->CreateICmpSLT(x, zero);

	llvm::Value *ret = jit->builder->CreateZExt(jit->builder->CreateExtractElement(cmp, static_cast<uint64_t>(0)), retTy);

	for(uint64_t i = 1, n = ty->getNumElements(); i < n; ++i)
	{
		llvm::Value *elem = jit->builder->CreateZExt(jit->builder->CreateExtractElement(cmp, i), retTy);

		ret = jit->builder->CreateOr(ret, jit->builder->CreateShl(elem, i));
	}

	return ret;
}
#endif

llvm::Value *lowerPUADDSAT(llvm::Value *x, llvm::Value *y)
{
	return jit->builder->CreateBinaryIntrinsic(llvm::Intrinsic::uadd_sat, x, y);
}

llvm::Value *lowerPSADDSAT(llvm::Value *x, llvm::Value *y)
{
	return jit->builder->CreateBinaryIntrinsic(llvm::Intrinsic::sadd_sat, x, y);
}

llvm::Value *lowerPUSUBSAT(llvm::Value *x, llvm::Value *y)
{
	return jit->builder->CreateBinaryIntrinsic(llvm::Intrinsic::usub_sat, x, y);
}

llvm::Value *lowerPSSUBSAT(llvm::Value *x, llvm::Value *y)
{
	return jit->builder->CreateBinaryIntrinsic(llvm::Intrinsic::ssub_sat, x, y);
}

llvm::Value *lowerMulHigh(llvm::Value *x, llvm::Value *y, bool sext)
{
	llvm::VectorType *ty = llvm::cast<llvm::VectorType>(x->getType());
	llvm::VectorType *extTy = llvm::VectorType::getExtendedElementVectorType(ty);

	llvm::Value *extX, *extY;
	if(sext)
	{
		extX = jit->builder->CreateSExt(x, extTy);
		extY = jit->builder->CreateSExt(y, extTy);
	}
	else
	{
		extX = jit->builder->CreateZExt(x, extTy);
		extY = jit->builder->CreateZExt(y, extTy);
	}

	llvm::Value *mult = jit->builder->CreateMul(extX, extY);

	llvm::IntegerType *intTy = llvm::cast<llvm::IntegerType>(ty->getElementType());
	llvm::Value *mulh = jit->builder->CreateAShr(mult, intTy->getBitWidth());

	return jit->builder->CreateTrunc(mulh, ty);
}

}

namespace rr {

std::string BackendName()
{
	return std::string("LLVM ") + LLVM_VERSION_STRING;
}

// The abstract Type* types are implemented as LLVM types, except that
// 64-bit vectors are emulated using 128-bit ones to avoid use of MMX in x86
// and VFP in ARM, and eliminate the overhead of converting them to explicit
// 128-bit ones. LLVM types are pointers, so we can represent emulated types
// as abstract pointers with small enum values.
enum InternalType : uintptr_t
{
	// Emulated types:
	Type_v2i32,
	Type_v4i16,
	Type_v2i16,
	Type_v8i8,
	Type_v4i8,
	Type_v2f32,
	EmulatedTypeCount,
	Type_LLVM // Returned by asInternalType() to indicate that the abstract Type* should be interpreted as LLVM type pointer
};

inline InternalType asInternalType(Type *type)
{
	InternalType t = static_cast<InternalType>(reinterpret_cast<uintptr_t>(type));
	return (t < EmulatedTypeCount) ? t : Type_LLVM;
}

llvm::Type *T(Type *t)
{
	// Use 128-bit vectors to implement logically shorter ones.
	switch(asInternalType(t))
	{
		case Type_v2i32: return T(Int4::type());
		case Type_v4i16: return T(Short8::type());
		case Type_v2i16: return T(Short8::type());
		case Type_v8i8:  return T(Byte16::type());
		case Type_v4i8:  return T(Byte16::type());
		case Type_v2f32: return T(Float4::type());
		case Type_LLVM:  return reinterpret_cast<llvm::Type *>(t);
		default:
			UNREACHABLE("asInternalType(t): %d", int(asInternalType(t)));
			return nullptr;
	}
}

Type *T(InternalType t)
{
	return reinterpret_cast<Type *>(t);
}

inline const std::vector<llvm::Type *> &T(const std::vector<Type *> &t)
{
	return reinterpret_cast<const std::vector<llvm::Type *> &>(t);
}

inline llvm::BasicBlock *B(BasicBlock *t)
{
	return reinterpret_cast<llvm::BasicBlock *>(t);
}

inline BasicBlock *B(llvm::BasicBlock *t)
{
	return reinterpret_cast<BasicBlock *>(t);
}

static size_t typeSize(Type *type)
{
	switch(asInternalType(type))
	{
		case Type_v2i32: return 8;
		case Type_v4i16: return 8;
		case Type_v2i16: return 4;
		case Type_v8i8:  return 8;
		case Type_v4i8:  return 4;
		case Type_v2f32: return 8;
		case Type_LLVM:
			{
				llvm::Type *t = T(type);

				if(t->isPointerTy())
				{
					return sizeof(void*);
				}

				// At this point we should only have LLVM 'primitive' types.
				unsigned int bits = t->getPrimitiveSizeInBits();
				ASSERT_MSG(bits != 0, "bits: %d", int(bits));

				// Booleans are 1 bit integers in LLVM's SSA type system,
				// but are typically stored as one byte.
				return (bits + 7) / 8;
			}
			break;
		default:
			UNREACHABLE("asInternalType(type): %d", int(asInternalType(type)));
			return 0;
	}
}

static unsigned int elementCount(Type *type)
{
	switch(asInternalType(type))
	{
		case Type_v2i32: return 2;
		case Type_v4i16: return 4;
		case Type_v2i16: return 2;
		case Type_v8i8:  return 8;
		case Type_v4i8:  return 4;
		case Type_v2f32: return 2;
		case Type_LLVM:  return llvm::cast<llvm::VectorType>(T(type))->getNumElements();
		default:
			UNREACHABLE("asInternalType(type): %d", int(asInternalType(type)));
			return 0;
	}
}

static ::llvm::Function *createFunction(const char *name, ::llvm::Type *retTy, const std::vector<::llvm::Type *> &params)
{
	llvm::FunctionType *functionType = llvm::FunctionType::get(retTy, params, false);
	auto func = llvm::Function::Create(functionType, llvm::GlobalValue::InternalLinkage, name, jit->module.get());

	func->setDoesNotThrow();
	func->setCallingConv(llvm::CallingConv::C);

	return func;
}

Nucleus::Nucleus()
{
	ASSERT(jit == nullptr);
	jit = new JITBuilder(Nucleus::getDefaultConfig());

	ASSERT(Variable::unmaterializedVariables == nullptr);
	Variable::unmaterializedVariables = new std::unordered_set<const Variable *>();
}

Nucleus::~Nucleus()
{
	delete Variable::unmaterializedVariables;
	Variable::unmaterializedVariables = nullptr;

	delete jit;
	jit = nullptr;
}

void Nucleus::adjustDefaultConfig(const Config::Edit &cfgEdit)
{
	std::unique_lock<std::mutex> lock(::defaultConfigLock);
	auto &config = ::defaultConfig();
	config = cfgEdit.apply(config);
}

Config Nucleus::getDefaultConfig()
{
	std::unique_lock<std::mutex> lock(::defaultConfigLock);
	return ::defaultConfig();
}

std::shared_ptr<Routine> Nucleus::acquireRoutine(const char *name, const Config::Edit &cfgEdit)
{
	if(jit->builder->GetInsertBlock()->empty() || !jit->builder->GetInsertBlock()->back().isTerminator())
	{
		llvm::Type *type = jit->function->getReturnType();

		if(type->isVoidTy())
		{
			createRetVoid();
		}
		else
		{
			createRet(V(llvm::UndefValue::get(type)));
		}
	}

	std::shared_ptr<Routine> routine;

	auto acquire = [&](JITBuilder *jit)
	{
		auto cfg = cfgEdit.apply(jit->config);

#ifdef ENABLE_RR_DEBUG_INFO
		if(jit->debugInfo != nullptr)
		{
			jit->debugInfo->Finalize();
		}
#endif

		if(false)
		{
			std::error_code error;
			llvm::raw_fd_ostream file(std::string(name) + "-llvm-dump-unopt.txt", error);
			jit->module->print(file, 0);
		}

#ifdef ENABLE_RR_LLVM_IR_VERIFICATION
		{
			llvm::legacy::PassManager pm;
			pm.add(llvm::createVerifierPass());
			pm.run(*jit->module);
		}
#endif

		jit->optimize(cfg);

		if(false)
		{
			std::error_code error;
			llvm::raw_fd_ostream file(std::string(name) + "-llvm-dump-opt.txt", error);
			jit->module->print(file, 0);
		}

		routine = jit->acquireRoutine(&jit->function, 1, cfg);
	};

	acquire(jit);

	return routine;
}

Value *Nucleus::allocateStackVariable(Type *type, int arraySize)
{
	llvm::BasicBlock &entryBlock = jit->function->getEntryBlock();

	llvm::Instruction *declaration;

#if LLVM_VERSION_MAJOR >= 11
	auto align = jit->module->getDataLayout().getPrefTypeAlign(T(type));
#else
	auto align = llvm::MaybeAlign(jit->module->getDataLayout().getPrefTypeAlignment(T(type)));
#endif

	if(arraySize)
	{
		declaration = new llvm::AllocaInst(T(type), 0, V(Nucleus::createConstantInt(arraySize)), align);
	}
	else
	{
		declaration = new llvm::AllocaInst(T(type), 0, (llvm::Value*)nullptr, align);
	}

	entryBlock.getInstList().push_front(declaration);

	return V(declaration);
}

BasicBlock *Nucleus::createBasicBlock()
{
	return B(llvm::BasicBlock::Create(jit->context, "", jit->function));
}

BasicBlock *Nucleus::getInsertBlock()
{
	return B(jit->builder->GetInsertBlock());
}

void Nucleus::setInsertBlock(BasicBlock *basicBlock)
{
	Variable::materializeAll();

	jit->builder->SetInsertPoint(B(basicBlock));
}

void Nucleus::createFunction(Type *ReturnType, const std::vector<Type *> &Params)
{
	jit->function = rr::createFunction("", T(ReturnType), T(Params));

#ifdef ENABLE_RR_DEBUG_INFO
	jit->debugInfo = std::make_unique<DebugInfo>(jit->builder.get(), &jit->context, jit->module.get(), jit->function);
#endif

	jit->builder->SetInsertPoint(llvm::BasicBlock::Create(jit->context, "", jit->function));
}

Value *Nucleus::getArgument(unsigned int index)
{
	llvm::Function::arg_iterator args = jit->function->arg_begin();

	while(index)
	{
		args++;
		index--;
	}

	return V(&*args);
}

void Nucleus::createRetVoid()
{
	RR_DEBUG_INFO_UPDATE_LOC();

	ASSERT_MSG(jit->function->getReturnType() == T(Void::type()), "Return type mismatch");

	// Code generated after this point is unreachable, so any variables
	// being read can safely return an undefined value. We have to avoid
	// materializing variables after the terminator ret instruction.
	Variable::killUnmaterialized();

	jit->builder->CreateRetVoid();
}

void Nucleus::createRet(Value *v)
{
	RR_DEBUG_INFO_UPDATE_LOC();

	ASSERT_MSG(jit->function->getReturnType() == V(v)->getType(), "Return type mismatch");

	// Code generated after this point is unreachable, so any variables
	// being read can safely return an undefined value. We have to avoid
	// materializing variables after the terminator ret instruction.
	Variable::killUnmaterialized();

	jit->builder->CreateRet(V(v));
}

void Nucleus::createBr(BasicBlock *dest)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	Variable::materializeAll();
	jit->builder->CreateBr(B(dest));
}

void Nucleus::createCondBr(Value *cond, BasicBlock *ifTrue, BasicBlock *ifFalse)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	Variable::materializeAll();
	jit->builder->CreateCondBr(V(cond), B(ifTrue), B(ifFalse));
}

Value *Nucleus::createAdd(Value *lhs, Value *rhs)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(jit->builder->CreateAdd(V(lhs), V(rhs)));
}

Value *Nucleus::createSub(Value *lhs, Value *rhs)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(jit->builder->CreateSub(V(lhs), V(rhs)));
}

Value *Nucleus::createMul(Value *lhs, Value *rhs)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(jit->builder->CreateMul(V(lhs), V(rhs)));
}

Value *Nucleus::createUDiv(Value *lhs, Value *rhs)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(jit->builder->CreateUDiv(V(lhs), V(rhs)));
}

Value *Nucleus::createSDiv(Value *lhs, Value *rhs)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(jit->builder->CreateSDiv(V(lhs), V(rhs)));
}

Value *Nucleus::createFAdd(Value *lhs, Value *rhs)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(jit->builder->CreateFAdd(V(lhs), V(rhs)));
}

Value *Nucleus::createFSub(Value *lhs, Value *rhs)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(jit->builder->CreateFSub(V(lhs), V(rhs)));
}

Value *Nucleus::createFMul(Value *lhs, Value *rhs)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(jit->builder->CreateFMul(V(lhs), V(rhs)));
}

Value *Nucleus::createFDiv(Value *lhs, Value *rhs)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(jit->builder->CreateFDiv(V(lhs), V(rhs)));
}

Value *Nucleus::createURem(Value *lhs, Value *rhs)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(jit->builder->CreateURem(V(lhs), V(rhs)));
}

Value *Nucleus::createSRem(Value *lhs, Value *rhs)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(jit->builder->CreateSRem(V(lhs), V(rhs)));
}

Value *Nucleus::createShl(Value *lhs, Value *rhs)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(jit->builder->CreateShl(V(lhs), V(rhs)));
}

Value *Nucleus::createLShr(Value *lhs, Value *rhs)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(jit->builder->CreateLShr(V(lhs), V(rhs)));
}

Value *Nucleus::createAShr(Value *lhs, Value *rhs)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(jit->builder->CreateAShr(V(lhs), V(rhs)));
}

Value *Nucleus::createAnd(Value *lhs, Value *rhs)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(jit->builder->CreateAnd(V(lhs), V(rhs)));
}

Value *Nucleus::createOr(Value *lhs, Value *rhs)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(jit->builder->CreateOr(V(lhs), V(rhs)));
}

Value *Nucleus::createXor(Value *lhs, Value *rhs)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(jit->builder->CreateXor(V(lhs), V(rhs)));
}

Value *Nucleus::createNeg(Value *v)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(jit->builder->CreateNeg(V(v)));
}

Value *Nucleus::createFNeg(Value *v)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(jit->builder->CreateFNeg(V(v)));
}

Value *Nucleus::createNot(Value *v)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(jit->builder->CreateNot(V(v)));
}

Value *Nucleus::createLoad(Value *ptr, Type *type, bool isVolatile, unsigned int alignment, bool atomic, std::memory_order memoryOrder)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	switch(asInternalType(type))
	{
		case Type_v2i32:
		case Type_v4i16:
		case Type_v8i8:
		case Type_v2f32:
			return createBitCast(createInsertElement(V(llvm::UndefValue::get(llvm::VectorType::get(T(Long::type()), 2))), createLoad(createBitCast(ptr, Pointer<Long>::type()), Long::type(), isVolatile, alignment, atomic, memoryOrder), 0), type);
		case Type_v2i16:
		case Type_v4i8:
			if(alignment != 0) // Not a local variable (all vectors are 128-bit).
			{
				Value *u = V(llvm::UndefValue::get(llvm::VectorType::get(T(Long::type()), 2)));
				Value *i = createLoad(createBitCast(ptr, Pointer<Int>::type()), Int::type(), isVolatile, alignment, atomic, memoryOrder);
				i = createZExt(i, Long::type());
				Value *v = createInsertElement(u, i, 0);
				return createBitCast(v, type);
			}
			// Fallthrough to non-emulated case.
		case Type_LLVM:
			{
				auto elTy = T(type);
				ASSERT(V(ptr)->getType()->getContainedType(0) == elTy);

				if(!atomic)
				{
					return V(jit->builder->CreateAlignedLoad(V(ptr), alignment, isVolatile));
				}
				else if(elTy->isIntegerTy() || elTy->isPointerTy())
				{
					// Integers and pointers can be atomically loaded by setting
					// the ordering constraint on the load instruction.
					auto load = jit->builder->CreateAlignedLoad(V(ptr), alignment, isVolatile);
					load->setAtomic(atomicOrdering(atomic, memoryOrder));
					return V(load);
				}
				else if(elTy->isFloatTy() || elTy->isDoubleTy())
				{
					// LLVM claims to support atomic loads of float types as
					// above, but certain backends cannot deal with this.
					// Load as an integer and bitcast.
					auto size = jit->module->getDataLayout().getTypeStoreSize(elTy);
					auto elAsIntTy = ::llvm::IntegerType::get(jit->context, size * 8);
					auto ptrCast = jit->builder->CreatePointerCast(V(ptr), elAsIntTy->getPointerTo());
					auto load = jit->builder->CreateAlignedLoad(ptrCast, alignment, isVolatile);
					load->setAtomic(atomicOrdering(atomic, memoryOrder));
					auto loadCast = jit->builder->CreateBitCast(load, elTy);
					return V(loadCast);
				}
				else
				{
					// More exotic types require falling back to the extern:
					// void __atomic_load(size_t size, void *ptr, void *ret, int ordering)
					auto sizetTy = ::llvm::IntegerType::get(jit->context, sizeof(size_t) * 8);
					auto intTy = ::llvm::IntegerType::get(jit->context, sizeof(int) * 8);
					auto i8Ty = ::llvm::Type::getInt8Ty(jit->context);
					auto i8PtrTy = i8Ty->getPointerTo();
					auto voidTy = ::llvm::Type::getVoidTy(jit->context);
					auto funcTy = ::llvm::FunctionType::get(voidTy, { sizetTy, i8PtrTy, i8PtrTy, intTy }, false);
					auto func = jit->module->getOrInsertFunction("__atomic_load", funcTy);
					auto size = jit->module->getDataLayout().getTypeStoreSize(elTy);
					auto out = allocateStackVariable(type);
					jit->builder->CreateCall(func, { ::llvm::ConstantInt::get(sizetTy, size), jit->builder->CreatePointerCast(V(ptr), i8PtrTy), jit->builder->CreatePointerCast(V(out), i8PtrTy), ::llvm::ConstantInt::get(intTy, uint64_t(atomicOrdering(true, memoryOrder))), });
					return V(jit->builder->CreateLoad(V(out)));
				}
			}
		default:
			UNREACHABLE("asInternalType(type): %d", int(asInternalType(type)));
			return nullptr;
	}
}

Value *Nucleus::createStore(Value *value, Value *ptr, Type *type, bool isVolatile, unsigned int alignment, bool atomic, std::memory_order memoryOrder)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	switch(asInternalType(type))
	{
		case Type_v2i32:
		case Type_v4i16:
		case Type_v8i8:
		case Type_v2f32:
			createStore(createExtractElement(createBitCast(value, T(llvm::VectorType::get(T(Long::type()), 2))), Long::type(), 0), createBitCast(ptr, Pointer<Long>::type()), Long::type(), isVolatile, alignment, atomic, memoryOrder);
			return value;
		case Type_v2i16:
		case Type_v4i8:
			if(alignment != 0) // Not a local variable (all vectors are 128-bit).
			{
				createStore(createExtractElement(createBitCast(value, Int4::type()), Int::type(), 0), createBitCast(ptr, Pointer<Int>::type()), Int::type(), isVolatile, alignment, atomic, memoryOrder);
				return value;
			}
			// Fallthrough to non-emulated case.
		case Type_LLVM:
			{
				auto elTy = T(type);
				ASSERT(V(ptr)->getType()->getContainedType(0) == elTy);

#if __has_feature(memory_sanitizer)
				// Mark all memory writes as initialized by calling __msan_unpoison
				{
					auto voidTy = ::llvm::Type::getVoidTy(jit->context);
					auto i8Ty = ::llvm::Type::getInt8Ty(jit->context);
					auto voidPtrTy = i8Ty->getPointerTo();
					auto sizetTy = ::llvm::IntegerType::get(jit->context, sizeof(size_t) * 8);
					auto funcTy = ::llvm::FunctionType::get(voidTy, { voidPtrTy, sizetTy }, false);
					auto func = jit->module->getOrInsertFunction("__msan_unpoison", funcTy);
					auto size = jit->module->getDataLayout().getTypeStoreSize(elTy);
					jit->builder->CreateCall(func, { jit->builder->CreatePointerCast(V(ptr), voidPtrTy), ::llvm::ConstantInt::get(sizetTy, size) });
				}
#endif

				if(!atomic)
				{
					jit->builder->CreateAlignedStore(V(value), V(ptr), alignment, isVolatile);
				}
				else if(elTy->isIntegerTy() || elTy->isPointerTy())
				{
					// Integers and pointers can be atomically stored by setting
					// the ordering constraint on the store instruction.
					auto store = jit->builder->CreateAlignedStore(V(value), V(ptr), alignment, isVolatile);
					store->setAtomic(atomicOrdering(atomic, memoryOrder));
				}
				else if(elTy->isFloatTy() || elTy->isDoubleTy())
				{
					// LLVM claims to support atomic stores of float types as
					// above, but certain backends cannot deal with this.
					// Store as an bitcast integer.
					auto size = jit->module->getDataLayout().getTypeStoreSize(elTy);
					auto elAsIntTy = ::llvm::IntegerType::get(jit->context, size * 8);
					auto valCast = jit->builder->CreateBitCast(V(value), elAsIntTy);
					auto ptrCast = jit->builder->CreatePointerCast(V(ptr), elAsIntTy->getPointerTo());
					auto store = jit->builder->CreateAlignedStore(valCast, ptrCast, alignment, isVolatile);
					store->setAtomic(atomicOrdering(atomic, memoryOrder));
				}
				else
				{
					// More exotic types require falling back to the extern:
					// void __atomic_store(size_t size, void *ptr, void *val, int ordering)
					auto sizetTy = ::llvm::IntegerType::get(jit->context, sizeof(size_t) * 8);
					auto intTy = ::llvm::IntegerType::get(jit->context, sizeof(int) * 8);
					auto i8Ty = ::llvm::Type::getInt8Ty(jit->context);
					auto i8PtrTy = i8Ty->getPointerTo();
					auto voidTy = ::llvm::Type::getVoidTy(jit->context);
					auto funcTy = ::llvm::FunctionType::get(voidTy, { sizetTy, i8PtrTy, i8PtrTy, intTy }, false);
					auto func = jit->module->getOrInsertFunction("__atomic_store", funcTy);
					auto size = jit->module->getDataLayout().getTypeStoreSize(elTy);
					auto copy = allocateStackVariable(type);
					jit->builder->CreateStore(V(value), V(copy));
					jit->builder->CreateCall(func, { ::llvm::ConstantInt::get(sizetTy, size), jit->builder->CreatePointerCast(V(ptr), i8PtrTy), jit->builder->CreatePointerCast(V(copy), i8PtrTy), ::llvm::ConstantInt::get(intTy, uint64_t(atomicOrdering(true, memoryOrder))), });
				}

				return value;
			}
		default:
			UNREACHABLE("asInternalType(type): %d", int(asInternalType(type)));
			return nullptr;
	}
}

Value *Nucleus::createGEP(Value *ptr, Type *type, Value *index, bool unsignedIndex)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	ASSERT(V(ptr)->getType()->getContainedType(0) == T(type));
	if(sizeof(void*) == 8)
	{
		index = unsignedIndex ? createZExt(index, Long::type()) : createSExt(index, Long::type());
	}

	// For non-emulated types we can rely on LLVM's GEP to calculate the
	// effective address correctly.
	if(asInternalType(type) == Type_LLVM)
	{
		return V(jit->builder->CreateGEP(V(ptr), V(index)));
	}

	// For emulated types we have to multiply the index by the intended
	// type size ourselves to obain the byte offset.
	index = (sizeof(void*) == 8) ? createMul(index, createConstantLong((int64_t)typeSize(type))) : createMul(index, createConstantInt((int)typeSize(type)));

	// Cast to a byte pointer, apply the byte offset, and cast back to the
	// original pointer type.
	return createBitCast(
	    V(jit->builder->CreateGEP(V(createBitCast(ptr, T(llvm::PointerType::get(T(Byte::type()), 0)))), V(index))),
	    T(llvm::PointerType::get(T(type), 0)));
}

Value *Nucleus::createTrunc(Value *v, Type *destType)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(jit->builder->CreateTrunc(V(v), T(destType)));
}

Value *Nucleus::createZExt(Value *v, Type *destType)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(jit->builder->CreateZExt(V(v), T(destType)));
}

Value *Nucleus::createSExt(Value *v, Type *destType)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(jit->builder->CreateSExt(V(v), T(destType)));
}

Value *Nucleus::createFPToUI(Value *v, Type *destType)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(jit->builder->CreateFPToUI(V(v), T(destType)));
}

Value *Nucleus::createFPToSI(Value *v, Type *destType)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(jit->builder->CreateFPToSI(V(v), T(destType)));
}

Value *Nucleus::createSIToFP(Value *v, Type *destType)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(jit->builder->CreateSIToFP(V(v), T(destType)));
}

Value *Nucleus::createBitCast(Value *v, Type *destType)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	// Bitcasts must be between types of the same logical size. But with emulated narrow vectors we need
	// support for casting between scalars and wide vectors. Emulate them by writing to the stack and
	// reading back as the destination type.
	if(!V(v)->getType()->isVectorTy() && T(destType)->isVectorTy())
	{
		Value *readAddress = allocateStackVariable(destType);
		Value *writeAddress = createBitCast(readAddress, T(llvm::PointerType::get(V(v)->getType(), 0)));
		createStore(v, writeAddress, T(V(v)->getType()));
		return createLoad(readAddress, destType);
	}
	else if(V(v)->getType()->isVectorTy() && !T(destType)->isVectorTy())
	{
		Value *writeAddress = allocateStackVariable(T(V(v)->getType()));
		createStore(v, writeAddress, T(V(v)->getType()));
		Value *readAddress = createBitCast(writeAddress, T(llvm::PointerType::get(T(destType), 0)));
		return createLoad(readAddress, destType);
	}

	return V(jit->builder->CreateBitCast(V(v), T(destType)));
}

Value *Nucleus::createICmpEQ(Value *lhs, Value *rhs)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(jit->builder->CreateICmpEQ(V(lhs), V(rhs)));
}

Value *Nucleus::createICmpNE(Value *lhs, Value *rhs)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(jit->builder->CreateICmpNE(V(lhs), V(rhs)));
}

Value *Nucleus::createICmpUGT(Value *lhs, Value *rhs)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(jit->builder->CreateICmpUGT(V(lhs), V(rhs)));
}

Value *Nucleus::createICmpUGE(Value *lhs, Value *rhs)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(jit->builder->CreateICmpUGE(V(lhs), V(rhs)));
}

Value *Nucleus::createICmpULT(Value *lhs, Value *rhs)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(jit->builder->CreateICmpULT(V(lhs), V(rhs)));
}

Value *Nucleus::createICmpULE(Value *lhs, Value *rhs)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(jit->builder->CreateICmpULE(V(lhs), V(rhs)));
}

Value *Nucleus::createICmpSGT(Value *lhs, Value *rhs)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(jit->builder->CreateICmpSGT(V(lhs), V(rhs)));
}

Value *Nucleus::createICmpSGE(Value *lhs, Value *rhs)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(jit->builder->CreateICmpSGE(V(lhs), V(rhs)));
}

Value *Nucleus::createICmpSLT(Value *lhs, Value *rhs)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(jit->builder->CreateICmpSLT(V(lhs), V(rhs)));
}

Value *Nucleus::createICmpSLE(Value *lhs, Value *rhs)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(jit->builder->CreateICmpSLE(V(lhs), V(rhs)));
}

Value *Nucleus::createFCmpOEQ(Value *lhs, Value *rhs)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(jit->builder->CreateFCmpOEQ(V(lhs), V(rhs)));
}

Value *Nucleus::createFCmpOGT(Value *lhs, Value *rhs)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(jit->builder->CreateFCmpOGT(V(lhs), V(rhs)));
}

Value *Nucleus::createFCmpOGE(Value *lhs, Value *rhs)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(jit->builder->CreateFCmpOGE(V(lhs), V(rhs)));
}

Value *Nucleus::createFCmpOLT(Value *lhs, Value *rhs)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(jit->builder->CreateFCmpOLT(V(lhs), V(rhs)));
}

Value *Nucleus::createFCmpOLE(Value *lhs, Value *rhs)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(jit->builder->CreateFCmpOLE(V(lhs), V(rhs)));
}

Value *Nucleus::createFCmpONE(Value *lhs, Value *rhs)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(jit->builder->CreateFCmpONE(V(lhs), V(rhs)));
}

Value *Nucleus::createExtractElement(Value *vector, Type *type, int index)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	ASSERT(V(vector)->getType()->getContainedType(0) == T(type));
	return V(jit->builder->CreateExtractElement(V(vector), V(createConstantInt(index))));
}

Value *Nucleus::createInsertElement(Value *vector, Value *element, int index)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(jit->builder->CreateInsertElement(V(vector), V(element), V(createConstantInt(index))));
}

Value *Nucleus::createShuffleVector(Value *v1, Value *v2, const int *select)
{
	RR_DEBUG_INFO_UPDATE_LOC();

	int size = llvm::cast<llvm::VectorType>(V(v1)->getType())->getNumElements();
	const int maxSize = 16;
	llvm::Constant *swizzle[maxSize];
	ASSERT(size <= maxSize);

	for(int i = 0; i < size; i++)
	{
		swizzle[i] = llvm::ConstantInt::get(llvm::Type::getInt32Ty(jit->context), select[i]);
	}

	llvm::Value *shuffle = llvm::ConstantVector::get(llvm::ArrayRef<llvm::Constant *>(swizzle, size));

	return V(jit->builder->CreateShuffleVector(V(v1), V(v2), shuffle));
}

Value *Nucleus::createSelect(Value *c, Value *ifTrue, Value *ifFalse)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(jit->builder->CreateSelect(V(c), V(ifTrue), V(ifFalse)));
}

SwitchCases *Nucleus::createSwitch(Value *control, BasicBlock *defaultBranch, unsigned numCases)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return reinterpret_cast<SwitchCases *>(jit->builder->CreateSwitch(V(control), B(defaultBranch), numCases));
}

void Nucleus::addSwitchCase(SwitchCases *switchCases, int label, BasicBlock *branch)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	llvm::SwitchInst *sw = reinterpret_cast<llvm::SwitchInst *>(switchCases);
	sw->addCase(llvm::ConstantInt::get(llvm::Type::getInt32Ty(jit->context), label, true), B(branch));
}

void Nucleus::createUnreachable()
{
	RR_DEBUG_INFO_UPDATE_LOC();
	jit->builder->CreateUnreachable();
}

Type *Nucleus::getPointerType(Type *ElementType)
{
	return T(llvm::PointerType::get(T(ElementType), 0));
}

Value *Nucleus::createNullValue(Type *Ty)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(llvm::Constant::getNullValue(T(Ty)));
}

Value *Nucleus::createConstantLong(int64_t i)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(llvm::ConstantInt::get(llvm::Type::getInt64Ty(jit->context), i, true));
}

Value *Nucleus::createConstantInt(int i)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(llvm::ConstantInt::get(llvm::Type::getInt32Ty(jit->context), i, true));
}

Value *Nucleus::createConstantInt(unsigned int i)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(llvm::ConstantInt::get(llvm::Type::getInt32Ty(jit->context), i, false));
}

Value *Nucleus::createConstantBool(bool b)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(llvm::ConstantInt::get(llvm::Type::getInt1Ty(jit->context), b));
}

Value *Nucleus::createConstantByte(signed char i)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(llvm::ConstantInt::get(llvm::Type::getInt8Ty(jit->context), i, true));
}

Value *Nucleus::createConstantByte(unsigned char i)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(llvm::ConstantInt::get(llvm::Type::getInt8Ty(jit->context), i, false));
}

Value *Nucleus::createConstantShort(short i)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(llvm::ConstantInt::get(llvm::Type::getInt16Ty(jit->context), i, true));
}

Value *Nucleus::createConstantShort(unsigned short i)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(llvm::ConstantInt::get(llvm::Type::getInt16Ty(jit->context), i, false));
}

Value *Nucleus::createConstantFloat(float x)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return V(llvm::ConstantFP::get(T(Float::type()), x));
}

Value *Nucleus::createConstantVector(const int64_t *constants, Type *type)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	ASSERT(llvm::isa<llvm::VectorType>(T(type)));
	const int numConstants = elementCount(type);
	const int numElements = llvm::cast<llvm::VectorType>(T(type))->getNumElements();
	ASSERT(numElements <= 16 && numConstants <= numElements);
	llvm::Constant *constantVector[16];

	for(int i = 0; i < numElements; i++)
	{
		constantVector[i] = llvm::ConstantInt::get(T(type)->getContainedType(0), constants[i % numConstants]);
	}

	return V(llvm::ConstantVector::get(llvm::ArrayRef<llvm::Constant *>(constantVector, numElements)));
}

Value *Nucleus::createConstantVector(const double *constants, Type *type)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	ASSERT(llvm::isa<llvm::VectorType>(T(type)));
	const int numConstants = elementCount(type);
	const int numElements = llvm::cast<llvm::VectorType>(T(type))->getNumElements();
	ASSERT(numElements <= 8 && numConstants <= numElements);
	llvm::Constant *constantVector[8];

	for(int i = 0; i < numElements; i++)
	{
		constantVector[i] = llvm::ConstantFP::get(T(type)->getContainedType(0), constants[i % numConstants]);
	}

	return V(llvm::ConstantVector::get(llvm::ArrayRef<llvm::Constant *>(constantVector, numElements)));
}

Value *Nucleus::createConstantString(const char *v)
{
	// Do not call RR_DEBUG_INFO_UPDATE_LOC() here to avoid recursion when called from Printv
	auto ptr = jit->builder->CreateGlobalStringPtr(v);
	return V(ptr);
}

Type *Void::type()
{
	return T(llvm::Type::getVoidTy(jit->context));
}

Type *Bool::type()
{
	return T(llvm::Type::getInt1Ty(jit->context));
}

Type *Byte::type()
{
	return T(llvm::Type::getInt8Ty(jit->context));
}

Type *SByte::type()
{
	return T(llvm::Type::getInt8Ty(jit->context));
}

Type *Short::type()
{
	return T(llvm::Type::getInt16Ty(jit->context));
}

Type *UShort::type()
{
	return T(llvm::Type::getInt16Ty(jit->context));
}

Type *Byte4::type()
{
	return T(Type_v4i8);
}

Type *SByte4::type()
{
	return T(Type_v4i8);
}

RValue<Byte8> AddSat(RValue<Byte8> x, RValue<Byte8> y)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	return x86::paddusb(x, y);
#else
	return As<Byte8>(V(lowerPUADDSAT(V(x.value()), V(y.value()))));
#endif
}

RValue<Byte8> SubSat(RValue<Byte8> x, RValue<Byte8> y)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	return x86::psubusb(x, y);
#else
	return As<Byte8>(V(lowerPUSUBSAT(V(x.value()), V(y.value()))));
#endif
}

RValue<Int> SignMask(RValue<Byte8> x)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	return x86::pmovmskb(x);
#else
	return As<Int>(V(lowerSignMask(V(x.value()), T(Int::type()))));
#endif
}

RValue<Byte8> CmpEQ(RValue<Byte8> x, RValue<Byte8> y)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	return x86::pcmpeqb(x, y);
#else
	return As<Byte8>(V(lowerPCMP(llvm::ICmpInst::ICMP_EQ, V(x.value()), V(y.value()), T(Byte8::type()))));
#endif
}

Type *Byte8::type()
{
	return T(Type_v8i8);
}

RValue<Int> SignMask(RValue<SByte8> x)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	return x86::pmovmskb(As<Byte8>(x));
#else
	return As<Int>(V(lowerSignMask(V(x.value()), T(Int::type()))));
#endif
}

RValue<Byte8> CmpGT(RValue<SByte8> x, RValue<SByte8> y)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	return x86::pcmpgtb(x, y);
#else
	return As<Byte8>(V(lowerPCMP(llvm::ICmpInst::ICMP_SGT, V(x.value()), V(y.value()), T(Byte8::type()))));
#endif
}

Type *SByte8::type()
{
	return T(Type_v8i8);
}

Type *Byte16::type()
{
	return T(llvm::VectorType::get(T(Byte::type()), 16));
}

Type *SByte16::type()
{
	return T(llvm::VectorType::get(T(SByte::type()), 16));
}

Type *Short2::type()
{
	return T(Type_v2i16);
}

Type *UShort2::type()
{
	return T(Type_v2i16);
}

Short4::Short4(RValue<Int4> cast)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	int select[8] = { 0, 2, 4, 6, 0, 2, 4, 6 };
	Value *short8 = Nucleus::createBitCast(cast.value(), Short8::type());

	Value *packed = Nucleus::createShuffleVector(short8, short8, select);
	Value *short4 = As<Short4>(Int2(As<Int4>(packed))).value();

	storeValue(short4);
}

RValue<Short4> operator<<(RValue<Short4> lhs, unsigned char rhs)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	return x86::psllw(lhs, rhs);
#else
	return As<Short4>(V(lowerVectorShl(V(lhs.value()), rhs)));
#endif
}

RValue<Short4> operator>>(RValue<Short4> lhs, unsigned char rhs)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	return x86::psraw(lhs, rhs);
#else
	return As<Short4>(V(lowerVectorAShr(V(lhs.value()), rhs)));
#endif
}

RValue<Short4> Max(RValue<Short4> x, RValue<Short4> y)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	return x86::pmaxsw(x, y);
#else
	return RValue<Short4>(V(lowerPMINMAX(V(x.value()), V(y.value()), llvm::ICmpInst::ICMP_SGT)));
#endif
}

RValue<Short4> Min(RValue<Short4> x, RValue<Short4> y)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	return x86::pminsw(x, y);
#else
	return RValue<Short4>(V(lowerPMINMAX(V(x.value()), V(y.value()), llvm::ICmpInst::ICMP_SLT)));
#endif
}

RValue<Short4> AddSat(RValue<Short4> x, RValue<Short4> y)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	return x86::paddsw(x, y);
#else
	return As<Short4>(V(lowerPSADDSAT(V(x.value()), V(y.value()))));
#endif
}

RValue<Short4> SubSat(RValue<Short4> x, RValue<Short4> y)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	return x86::psubsw(x, y);
#else
	return As<Short4>(V(lowerPSSUBSAT(V(x.value()), V(y.value()))));
#endif
}

RValue<Short4> MulHigh(RValue<Short4> x, RValue<Short4> y)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	return x86::pmulhw(x, y);
#else
	return As<Short4>(V(lowerMulHigh(V(x.value()), V(y.value()), true)));
#endif
}

RValue<Int2> MulAdd(RValue<Short4> x, RValue<Short4> y)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	return x86::pmaddwd(x, y);
#else
	return As<Int2>(V(lowerMulAdd(V(x.value()), V(y.value()))));
#endif
}

RValue<SByte8> PackSigned(RValue<Short4> x, RValue<Short4> y)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	auto result = x86::packsswb(x, y);
#else
	auto result = V(lowerPack(V(x.value()), V(y.value()), true));
#endif
	return As<SByte8>(Swizzle(As<Int4>(result), 0x0202));
}

RValue<Byte8> PackUnsigned(RValue<Short4> x, RValue<Short4> y)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	auto result = x86::packuswb(x, y);
#else
	auto result = V(lowerPack(V(x.value()), V(y.value()), false));
#endif
	return As<Byte8>(Swizzle(As<Int4>(result), 0x0202));
}

RValue<Short4> CmpGT(RValue<Short4> x, RValue<Short4> y)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	return x86::pcmpgtw(x, y);
#else
	return As<Short4>(V(lowerPCMP(llvm::ICmpInst::ICMP_SGT, V(x.value()), V(y.value()), T(Short4::type()))));
#endif
}

RValue<Short4> CmpEQ(RValue<Short4> x, RValue<Short4> y)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	return x86::pcmpeqw(x, y);
#else
	return As<Short4>(V(lowerPCMP(llvm::ICmpInst::ICMP_EQ, V(x.value()), V(y.value()), T(Short4::type()))));
#endif
}

Type *Short4::type()
{
	return T(Type_v4i16);
}

UShort4::UShort4(RValue<Float4> cast, bool saturate)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	if(saturate)
	{
#if defined(__i386__) || defined(__x86_64__)
		if(hasSSE41)
		{
			Int4 int4(Min(cast, Float4(0xFFFF)));
			*this = As<Short4>(PackUnsigned(int4, int4));
		}
		else
#endif
		{
			*this = Short4(Int4(Max(Min(cast, Float4(0xFFFF)), Float4(0x0000))));
		}
	}
	else
	{
		*this = Short4(Int4(cast));
	}
}

RValue<UShort4> operator<<(RValue<UShort4> lhs, unsigned char rhs)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	return As<UShort4>(x86::psllw(As<Short4>(lhs), rhs));
#else
	return As<UShort4>(V(lowerVectorShl(V(lhs.value()), rhs)));
#endif
}

RValue<UShort4> operator>>(RValue<UShort4> lhs, unsigned char rhs)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	return x86::psrlw(lhs, rhs);
#else
	return As<UShort4>(V(lowerVectorLShr(V(lhs.value()), rhs)));
#endif
}

RValue<UShort4> Max(RValue<UShort4> x, RValue<UShort4> y)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return RValue<UShort4>(Max(As<Short4>(x) - Short4(0x8000u, 0x8000u, 0x8000u, 0x8000u), As<Short4>(y) - Short4(0x8000u, 0x8000u, 0x8000u, 0x8000u)) + Short4(0x8000u, 0x8000u, 0x8000u, 0x8000u));
}

RValue<UShort4> Min(RValue<UShort4> x, RValue<UShort4> y)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return RValue<UShort4>(Min(As<Short4>(x) - Short4(0x8000u, 0x8000u, 0x8000u, 0x8000u), As<Short4>(y) - Short4(0x8000u, 0x8000u, 0x8000u, 0x8000u)) + Short4(0x8000u, 0x8000u, 0x8000u, 0x8000u));
}

RValue<UShort4> AddSat(RValue<UShort4> x, RValue<UShort4> y)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	return x86::paddusw(x, y);
#else
	return As<UShort4>(V(lowerPUADDSAT(V(x.value()), V(y.value()))));
#endif
}

RValue<UShort4> SubSat(RValue<UShort4> x, RValue<UShort4> y)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	return x86::psubusw(x, y);
#else
	return As<UShort4>(V(lowerPUSUBSAT(V(x.value()), V(y.value()))));
#endif
}

RValue<UShort4> MulHigh(RValue<UShort4> x, RValue<UShort4> y)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	return x86::pmulhuw(x, y);
#else
	return As<UShort4>(V(lowerMulHigh(V(x.value()), V(y.value()), false)));
#endif
}

Type *UShort4::type()
{
	return T(Type_v4i16);
}

Type *Short8::type()
{
	return T(llvm::VectorType::get(T(Short::type()), 8));
}

Type *UShort8::type()
{
	return T(llvm::VectorType::get(T(UShort::type()), 8));
}

RValue<Int> operator++(Int &val, int) // Post-increment
{
	RR_DEBUG_INFO_UPDATE_LOC();
	RValue<Int> res = val;

	Value *inc = Nucleus::createAdd(res.value(), Nucleus::createConstantInt(1));
	val.storeValue(inc);

	return res;
}

RValue<Int> operator--(Int &val, int) // Post-decrement
{
	RR_DEBUG_INFO_UPDATE_LOC();
	RValue<Int> res = val;

	Value *inc = Nucleus::createSub(res.value(), Nucleus::createConstantInt(1));
	val.storeValue(inc);

	return res;
}

const Int &operator--(Int &val) // Pre-decrement
{
	RR_DEBUG_INFO_UPDATE_LOC();
	Value *inc = Nucleus::createSub(val.loadValue(), Nucleus::createConstantInt(1));
	val.storeValue(inc);

	return val;
}

RValue<Int> RoundInt(RValue<Float> cast)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	return x86::cvtss2si(cast);
#else
	return RValue<Int>(V(lowerRoundInt(V(cast.value()), T(Int::type()))));
#endif
}

Type *Int::type()
{
	return T(llvm::Type::getInt32Ty(jit->context));
}

Type *Long::type()
{
	return T(llvm::Type::getInt64Ty(jit->context));
}

RValue<UInt> operator++(UInt &val, int) // Post-increment
{
	RR_DEBUG_INFO_UPDATE_LOC();
	RValue<UInt> res = val;

	Value *inc = Nucleus::createAdd(res.value(), Nucleus::createConstantInt(1));
	val.storeValue(inc);

	return res;
}

RValue<UInt> operator--(UInt &val, int) // Post-decrement
{
	RR_DEBUG_INFO_UPDATE_LOC();
	RValue<UInt> res = val;

	Value *inc = Nucleus::createSub(res.value(), Nucleus::createConstantInt(1));
	val.storeValue(inc);

	return res;
}

Type *UInt::type()
{
	return T(llvm::Type::getInt32Ty(jit->context));
}

Type *Int2::type()
{
	return T(Type_v2i32);
}

Type *UInt2::type()
{
	return T(Type_v2i32);
}

Int4::Int4(RValue<Byte4> cast) : XYZW(this)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	if(hasSSE41)
	{
		*this = x86::pmovzxbd(As<Byte16>(cast));
	}
	else
#endif
	{
		int swizzle[16] = { 0, 16, 1, 17, 2, 18, 3, 19, 4, 20, 5, 21, 6, 22, 7, 23 };
		Value *a = Nucleus::createBitCast(cast.value(), Byte16::type());
		Value *b = Nucleus::createShuffleVector(a, Nucleus::createNullValue(Byte16::type()), swizzle);

		int swizzle2[8] = { 0, 8, 1, 9, 2, 10, 3, 11 };
		Value *c = Nucleus::createBitCast(b, Short8::type());
		Value *d = Nucleus::createShuffleVector(c, Nucleus::createNullValue(Short8::type()), swizzle2);

		*this = As<Int4>(d);
	}
}

Int4::Int4(RValue<SByte4> cast) : XYZW(this)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	if(hasSSE41)
	{
		*this = x86::pmovsxbd(As<SByte16>(cast));
	}
	else
#endif
	{
		int swizzle[16] = { 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7 };
		Value *a = Nucleus::createBitCast(cast.value(), Byte16::type());
		Value *b = Nucleus::createShuffleVector(a, a, swizzle);

		int swizzle2[8] = { 0, 0, 1, 1, 2, 2, 3, 3 };
		Value *c = Nucleus::createBitCast(b, Short8::type());
		Value *d = Nucleus::createShuffleVector(c, c, swizzle2);

		*this = As<Int4>(d) >> 24;
	}
}

Int4::Int4(RValue<Short4> cast) : XYZW(this)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	if(hasSSE41)
	{
		*this = x86::pmovsxwd(As<Short8>(cast));
	}
	else
#endif
	{
		int swizzle[8] = { 0, 0, 1, 1, 2, 2, 3, 3 };
		Value *c = Nucleus::createShuffleVector(cast.value(), cast.value(), swizzle);
		*this = As<Int4>(c) >> 16;
	}
}

Int4::Int4(RValue<UShort4> cast) : XYZW(this)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	if(hasSSE41)
	{
		*this = x86::pmovzxwd(As<UShort8>(cast));
	}
	else
#endif
	{
		int swizzle[8] = { 0, 8, 1, 9, 2, 10, 3, 11 };
		Value *c = Nucleus::createShuffleVector(cast.value(), Short8(0, 0, 0, 0, 0, 0, 0, 0).loadValue(), swizzle);
		*this = As<Int4>(c);
	}
}

Int4::Int4(RValue<Int> rhs) : XYZW(this)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	Value *vector = loadValue();
	Value *insert = Nucleus::createInsertElement(vector, rhs.value(), 0);

	int swizzle[4] = { 0, 0, 0, 0 };
	Value *replicate = Nucleus::createShuffleVector(insert, insert, swizzle);

	storeValue(replicate);
}

RValue<Int4> operator<<(RValue<Int4> lhs, unsigned char rhs)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	return x86::pslld(lhs, rhs);
#else
	return As<Int4>(V(lowerVectorShl(V(lhs.value()), rhs)));
#endif
}

RValue<Int4> operator>>(RValue<Int4> lhs, unsigned char rhs)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	return x86::psrad(lhs, rhs);
#else
	return As<Int4>(V(lowerVectorAShr(V(lhs.value()), rhs)));
#endif
}

RValue<Int4> CmpEQ(RValue<Int4> x, RValue<Int4> y)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return RValue<Int4>(Nucleus::createSExt(Nucleus::createICmpEQ(x.value(), y.value()), Int4::type()));
}

RValue<Int4> CmpLT(RValue<Int4> x, RValue<Int4> y)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return RValue<Int4>(Nucleus::createSExt(Nucleus::createICmpSLT(x.value(), y.value()), Int4::type()));
}

RValue<Int4> CmpLE(RValue<Int4> x, RValue<Int4> y)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return RValue<Int4>(Nucleus::createSExt(Nucleus::createICmpSLE(x.value(), y.value()), Int4::type()));
}

RValue<Int4> CmpNEQ(RValue<Int4> x, RValue<Int4> y)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return RValue<Int4>(Nucleus::createSExt(Nucleus::createICmpNE(x.value(), y.value()), Int4::type()));
}

RValue<Int4> CmpNLT(RValue<Int4> x, RValue<Int4> y)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return RValue<Int4>(Nucleus::createSExt(Nucleus::createICmpSGE(x.value(), y.value()), Int4::type()));
}

RValue<Int4> CmpNLE(RValue<Int4> x, RValue<Int4> y)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return RValue<Int4>(Nucleus::createSExt(Nucleus::createICmpSGT(x.value(), y.value()), Int4::type()));
}

RValue<Int4> Max(RValue<Int4> x, RValue<Int4> y)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	if(hasSSE41)
	{
		return x86::pmaxsd(x, y);
	}
	else
#endif
	{
		RValue<Int4> greater = CmpNLE(x, y);
		return (x & greater) | (y & ~greater);
	}
}

RValue<Int4> Min(RValue<Int4> x, RValue<Int4> y)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	if(hasSSE41)
	{
		return x86::pminsd(x, y);
	}
	else
#endif
	{
		RValue<Int4> less = CmpLT(x, y);
		return (x & less) | (y & ~less);
	}
}

RValue<Int4> RoundInt(RValue<Float4> cast)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	return x86::cvtps2dq(cast);
#else
	return As<Int4>(V(lowerRoundInt(V(cast.value()), T(Int4::type()))));
#endif
}

RValue<UInt4> MulHigh(RValue<UInt4> x, RValue<UInt4> y)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return As<UInt4>(V(lowerMulHigh(V(x.value()), V(y.value()), false)));
}

RValue<Short8> PackSigned(RValue<Int4> x, RValue<Int4> y)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	return x86::packssdw(x, y);
#else
	return As<Short8>(V(lowerPack(V(x.value()), V(y.value()), true)));
#endif
}

RValue<UShort8> PackUnsigned(RValue<Int4> x, RValue<Int4> y)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	return x86::packusdw(x, y);
#else
	return As<UShort8>(V(lowerPack(V(x.value()), V(y.value()), false)));
#endif
}

RValue<Int> SignMask(RValue<Int4> x)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	return x86::movmskps(As<Float4>(x));
#else
	return As<Int>(V(lowerSignMask(V(x.value()), T(Int::type()))));
#endif
}

Type *Int4::type()
{
	return T(llvm::VectorType::get(T(Int::type()), 4));
}

UInt4::UInt4(RValue<Float4> cast) : XYZW(this)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	Value *xyzw = Nucleus::createFPToUI(cast.value(), UInt4::type());
	storeValue(xyzw);
}

RValue<UInt4> operator<<(RValue<UInt4> lhs, unsigned char rhs)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	return As<UInt4>(x86::pslld(As<Int4>(lhs), rhs));
#else
	return As<UInt4>(V(lowerVectorShl(V(lhs.value()), rhs)));
#endif
}

RValue<UInt4> operator>>(RValue<UInt4> lhs, unsigned char rhs)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	return x86::psrld(lhs, rhs);
#else
	return As<UInt4>(V(lowerVectorLShr(V(lhs.value()), rhs)));
#endif
}

RValue<UInt4> CmpEQ(RValue<UInt4> x, RValue<UInt4> y)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return RValue<UInt4>(Nucleus::createSExt(Nucleus::createICmpEQ(x.value(), y.value()), Int4::type()));
}

RValue<UInt4> CmpLT(RValue<UInt4> x, RValue<UInt4> y)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return RValue<UInt4>(Nucleus::createSExt(Nucleus::createICmpULT(x.value(), y.value()), Int4::type()));
}

RValue<UInt4> CmpLE(RValue<UInt4> x, RValue<UInt4> y)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return RValue<UInt4>(Nucleus::createSExt(Nucleus::createICmpULE(x.value(), y.value()), Int4::type()));
}

RValue<UInt4> CmpNEQ(RValue<UInt4> x, RValue<UInt4> y)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return RValue<UInt4>(Nucleus::createSExt(Nucleus::createICmpNE(x.value(), y.value()), Int4::type()));
}

RValue<UInt4> CmpNLT(RValue<UInt4> x, RValue<UInt4> y)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return RValue<UInt4>(Nucleus::createSExt(Nucleus::createICmpUGE(x.value(), y.value()), Int4::type()));
}

RValue<UInt4> CmpNLE(RValue<UInt4> x, RValue<UInt4> y)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return RValue<UInt4>(Nucleus::createSExt(Nucleus::createICmpUGT(x.value(), y.value()), Int4::type()));
}

RValue<UInt4> Min(RValue<UInt4> x, RValue<UInt4> y)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	if(hasSSE41)
	{
		return x86::pminud(x, y);
	}
	else
#endif
	{
		RValue<UInt4> less = CmpLT(x, y);
		return (x & less) | (y & ~less);
	}
}

Type *UInt4::type()
{
	return T(llvm::VectorType::get(T(UInt::type()), 4));
}

RValue<Float> Rcp_pp(RValue<Float> x, bool exactAtPow2)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	if(exactAtPow2)
	{
		// rcpss uses a piecewise-linear approximation which minimizes the relative error
		// but is not exact at power-of-two values. Rectify by multiplying by the inverse.
		return x86::rcpss(x) * Float(1.0f / _mm_cvtss_f32(_mm_rcp_ss(_mm_set_ps1(1.0f))));
	}
	return x86::rcpss(x);
#else
	return As<Float>(V(lowerRCP(V(x.value()))));
#endif
}

RValue<Float> Frac(RValue<Float> x)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	if(hasSSE41)
	{
		return x - x86::floorss(x);
	}
	else
	{
		return Float4(Frac(Float4(x))).x;
	}
#else
	// x - floor(x) can be 1.0 for very small negative x.
	// Clamp against the value just below 1.0.
	return Min(x - Floor(x), As<Float>(Int(0x3F7FFFFF)));
#endif
}

RValue<Float> Floor(RValue<Float> x)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	if(hasSSE41)
	{
		return x86::floorss(x);
	}
	else
	{
		return Float4(Floor(Float4(x))).x;
	}
#else
	return RValue<Float>(V(lowerFloor(V(x.value()))));
#endif
}

Type *Float::type()
{
	return T(llvm::Type::getFloatTy(jit->context));
}

Type *Float2::type()
{
	return T(Type_v2f32);
}

Float4::Float4(RValue<Float> rhs) : XYZW(this)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	Value *vector = loadValue();
	Value *insert = Nucleus::createInsertElement(vector, rhs.value(), 0);

	int swizzle[4] = { 0, 0, 0, 0 };
	Value *replicate = Nucleus::createShuffleVector(insert, insert, swizzle);

	storeValue(replicate);
}

RValue<Float4> Max(RValue<Float4> x, RValue<Float4> y)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	return x86::maxps(x, y);
#else
	return As<Float4>(V(lowerPFMINMAX(V(x.value()), V(y.value()), llvm::FCmpInst::FCMP_OGT)));
#endif
}

RValue<Float4> Min(RValue<Float4> x, RValue<Float4> y)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	return x86::minps(x, y);
#else
	return As<Float4>(V(lowerPFMINMAX(V(x.value()), V(y.value()), llvm::FCmpInst::FCMP_OLT)));
#endif
}

RValue<Float4> Rcp_pp(RValue<Float4> x, bool exactAtPow2)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	if(exactAtPow2)
	{
		// rcpps uses a piecewise-linear approximation which minimizes the relative error
		// but is not exact at power-of-two values. Rectify by multiplying by the inverse.
		return x86::rcpps(x) * Float4(1.0f / _mm_cvtss_f32(_mm_rcp_ss(_mm_set_ps1(1.0f))));
	}
	return x86::rcpps(x);
#else
	return As<Float4>(V(lowerRCP(V(x.value()))));
#endif
}

RValue<Float4> RcpSqrt_pp(RValue<Float4> x)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	return x86::rsqrtps(x);
#else
	return As<Float4>(V(lowerRSQRT(V(x.value()))));
#endif
}

RValue<Float4> Sqrt(RValue<Float4> x)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	return x86::sqrtps(x);
#else
	return As<Float4>(V(lowerSQRT(V(x.value()))));
#endif
}

RValue<Int4> CmpEQ(RValue<Float4> x, RValue<Float4> y)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return RValue<Int4>(Nucleus::createSExt(Nucleus::createFCmpOEQ(x.value(), y.value()), Int4::type()));
}

RValue<Int4> CmpLT(RValue<Float4> x, RValue<Float4> y)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return RValue<Int4>(Nucleus::createSExt(Nucleus::createFCmpOLT(x.value(), y.value()), Int4::type()));
}

RValue<Int4> CmpLE(RValue<Float4> x, RValue<Float4> y)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return RValue<Int4>(Nucleus::createSExt(Nucleus::createFCmpOLE(x.value(), y.value()), Int4::type()));
}

RValue<Int4> CmpNEQ(RValue<Float4> x, RValue<Float4> y)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return RValue<Int4>(Nucleus::createSExt(Nucleus::createFCmpONE(x.value(), y.value()), Int4::type()));
}

RValue<Int4> CmpNLT(RValue<Float4> x, RValue<Float4> y)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return RValue<Int4>(Nucleus::createSExt(Nucleus::createFCmpOGE(x.value(), y.value()), Int4::type()));
}

RValue<Int4> CmpNLE(RValue<Float4> x, RValue<Float4> y)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	return RValue<Int4>(Nucleus::createSExt(Nucleus::createFCmpOGT(x.value(), y.value()), Int4::type()));
}

RValue<Float4> Round(RValue<Float4> x)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	if(hasSSE41)
	{
		return x86::roundps(x, 0);
	}
	else
	{
		return Float4(RoundInt(x));
	}
#else
	return RValue<Float4>(V(lowerRound(V(x.value()))));
#endif
}

RValue<Float4> Trunc(RValue<Float4> x)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	if(hasSSE41)
	{
		return x86::roundps(x, 3);
	}
	else
	{
		return Float4(Int4(x));
	}
#else
	return RValue<Float4>(V(lowerTrunc(V(x.value()))));
#endif
}

RValue<Float4> Frac(RValue<Float4> x)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	Float4 frc;

#if defined(__i386__) || defined(__x86_64__)
	if(hasSSE41)
	{
		frc = x - Floor(x);
	}
	else
	{
		frc = x - Float4(Int4(x)); // Signed fractional part.

		frc += As<Float4>(As<Int4>(CmpNLE(Float4(0.0f), frc)) & As<Int4>(Float4(1.0f)));  // Add 1.0 if negative.
	}
#else
	frc = x - Floor(x);
#endif

	// x - floor(x) can be 1.0 for very small negative x.
	// Clamp against the value just below 1.0.
	return Min(frc, As<Float4>(Int4(0x3F7FFFFF)));
}

RValue<Float4> Floor(RValue<Float4> x)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	if(hasSSE41)
	{
		return x86::floorps(x);
	}
	else
	{
		return x - Frac(x);
	}
#else
	return RValue<Float4>(V(lowerFloor(V(x.value()))));
#endif
}

RValue<Float4> Ceil(RValue<Float4> x)
{
	RR_DEBUG_INFO_UPDATE_LOC();
#if defined(__i386__) || defined(__x86_64__)
	if(hasSSE41)
	{
		return x86::ceilps(x);
	}
	else
#endif
	{
		return -Floor(-x);
	}
}

Type *Float4::type()
{
	return T(llvm::VectorType::get(T(Float::type()), 4));
}

RValue<Long> Ticks()
{
	RR_DEBUG_INFO_UPDATE_LOC();
	llvm::Function *rdtsc = llvm::Intrinsic::getDeclaration(jit->module.get(), llvm::Intrinsic::readcyclecounter);

	return RValue<Long>(V(jit->builder->CreateCall(rdtsc)));
}

RValue<Pointer<Byte>> ConstantPointer(void const *ptr)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	auto ptrAsInt = ::llvm::ConstantInt::get(::llvm::Type::getInt64Ty(jit->context), reinterpret_cast<uintptr_t>(ptr));
	return RValue<Pointer<Byte>>(V(jit->builder->CreateIntToPtr(ptrAsInt, T(Pointer<Byte>::type()))));
}

Value *Call(RValue<Pointer<Byte>> fptr, Type *retTy, std::initializer_list<Value *> args, std::initializer_list<Type *> argTys)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	::llvm::SmallVector<::llvm::Type *, 8> paramTys;
	for(auto ty : argTys) { paramTys.push_back(T(ty)); }
	auto funcTy = ::llvm::FunctionType::get(T(retTy), paramTys, false);

	auto funcPtrTy = funcTy->getPointerTo();
	auto funcPtr = jit->builder->CreatePointerCast(V(fptr.value()), funcPtrTy);

	::llvm::SmallVector<::llvm::Value *, 8> arguments;
	for(auto arg : args) { arguments.push_back(V(arg)); }
	return V(jit->builder->CreateCall(funcTy, funcPtr, arguments));
}

#if defined(__i386__) || defined(__x86_64__)
namespace x86 {

// Differs from IRBuilder<>::CreateUnaryIntrinsic() in that it only accepts native instruction intrinsics which have
// implicit types, such as 'x86_sse_rcp_ps' operating on v4f32, while 'sqrt' requires explicitly specifying the operand type.
static Value *createInstruction(llvm::Intrinsic::ID id, Value *x)
{
	llvm::Function *intrinsic = llvm::Intrinsic::getDeclaration(jit->module.get(), id);

	return V(jit->builder->CreateCall(intrinsic, V(x)));
}

// Differs from IRBuilder<>::CreateBinaryIntrinsic() in that it only accepts native instruction intrinsics which have
// implicit types, such as 'x86_sse_max_ps' operating on v4f32, while 'sadd_sat' requires explicitly specifying the operand types.
static Value *createInstruction(llvm::Intrinsic::ID id, Value *x, Value *y)
{
	llvm::Function *intrinsic = llvm::Intrinsic::getDeclaration(jit->module.get(), id);

	return V(jit->builder->CreateCall(intrinsic, { V(x), V(y) }));
}

RValue<Int> cvtss2si(RValue<Float> val)
{
	Float4 vector;
	vector.x = val;

	return RValue<Int>(createInstruction(llvm::Intrinsic::x86_sse_cvtss2si, RValue<Float4>(vector).value()));
}

RValue<Int4> cvtps2dq(RValue<Float4> val)
{
	return RValue<Int4>(createInstruction(llvm::Intrinsic::x86_sse2_cvtps2dq, val.value()));
}

RValue<Float> rcpss(RValue<Float> val)
{
	Value *vector = Nucleus::createInsertElement(V(llvm::UndefValue::get(T(Float4::type()))), val.value(), 0);

	return RValue<Float>(Nucleus::createExtractElement(createInstruction(llvm::Intrinsic::x86_sse_rcp_ss, vector), Float::type(), 0));
}

RValue<Float4> rcpps(RValue<Float4> val)
{
	return RValue<Float4>(createInstruction(llvm::Intrinsic::x86_sse_rcp_ps, val.value()));
}

RValue<Float4> sqrtps(RValue<Float4> val)
{
	return RValue<Float4>(V(jit->builder->CreateUnaryIntrinsic(llvm::Intrinsic::sqrt, V(val.value()))));
}

RValue<Float4> rsqrtps(RValue<Float4> val)
{
	return RValue<Float4>(createInstruction(llvm::Intrinsic::x86_sse_rsqrt_ps, val.value()));
}

RValue<Float4> maxps(RValue<Float4> x, RValue<Float4> y)
{
	return RValue<Float4>(createInstruction(llvm::Intrinsic::x86_sse_max_ps, x.value(), y.value()));
}

RValue<Float4> minps(RValue<Float4> x, RValue<Float4> y)
{
	return RValue<Float4>(createInstruction(llvm::Intrinsic::x86_sse_min_ps, x.value(), y.value()));
}

RValue<Float> roundss(RValue<Float> val, unsigned char imm)
{
	llvm::Function *roundss = llvm::Intrinsic::getDeclaration(jit->module.get(), llvm::Intrinsic::x86_sse41_round_ss);

	Value *undef = V(llvm::UndefValue::get(T(Float4::type())));
	Value *vector = Nucleus::createInsertElement(undef, val.value(), 0);

	return RValue<Float>(Nucleus::createExtractElement(V(jit->builder->CreateCall(roundss, { V(undef), V(vector), V(Nucleus::createConstantInt(imm)) })), Float::type(), 0));
}

RValue<Float> floorss(RValue<Float> val)
{
	return roundss(val, 1);
}

RValue<Float4> roundps(RValue<Float4> val, unsigned char imm)
{
	return RValue<Float4>(createInstruction(llvm::Intrinsic::x86_sse41_round_ps, val.value(), Nucleus::createConstantInt(imm)));
}

RValue<Float4> floorps(RValue<Float4> val)
{
	return roundps(val, 1);
}

RValue<Float4> ceilps(RValue<Float4> val)
{
	return roundps(val, 2);
}

RValue<Short4> paddsw(RValue<Short4> x, RValue<Short4> y)
{
	return As<Short4>(V(lowerPSADDSAT(V(x.value()), V(y.value()))));
}

RValue<Short4> psubsw(RValue<Short4> x, RValue<Short4> y)
{
	return As<Short4>(V(lowerPSSUBSAT(V(x.value()), V(y.value()))));
}

RValue<UShort4> paddusw(RValue<UShort4> x, RValue<UShort4> y)
{
	return As<UShort4>(V(lowerPUADDSAT(V(x.value()), V(y.value()))));
}

RValue<UShort4> psubusw(RValue<UShort4> x, RValue<UShort4> y)
{
	return As<UShort4>(V(lowerPUSUBSAT(V(x.value()), V(y.value()))));
}

RValue<Byte8> paddusb(RValue<Byte8> x, RValue<Byte8> y)
{
	return As<Byte8>(V(lowerPUADDSAT(V(x.value()), V(y.value()))));
}

RValue<Byte8> psubusb(RValue<Byte8> x, RValue<Byte8> y)
{
	return As<Byte8>(V(lowerPUSUBSAT(V(x.value()), V(y.value()))));
}

RValue<Short4> pmaxsw(RValue<Short4> x, RValue<Short4> y)
{
	return As<Short4>(V(lowerPMINMAX(V(x.value()), V(y.value()), llvm::ICmpInst::ICMP_SGT)));
}

RValue<Short4> pminsw(RValue<Short4> x, RValue<Short4> y)
{
	return As<Short4>(V(lowerPMINMAX(V(x.value()), V(y.value()), llvm::ICmpInst::ICMP_SLT)));
}

RValue<Short4> pcmpgtw(RValue<Short4> x, RValue<Short4> y)
{
	return As<Short4>(V(lowerPCMP(llvm::ICmpInst::ICMP_SGT, V(x.value()), V(y.value()), T(Short4::type()))));
}

RValue<Short4> pcmpeqw(RValue<Short4> x, RValue<Short4> y)
{
	return As<Short4>(V(lowerPCMP(llvm::ICmpInst::ICMP_EQ, V(x.value()), V(y.value()), T(Short4::type()))));
}

RValue<Byte8> pcmpgtb(RValue<SByte8> x, RValue<SByte8> y)
{
	return As<Byte8>(V(lowerPCMP(llvm::ICmpInst::ICMP_SGT, V(x.value()), V(y.value()), T(Byte8::type()))));
}

RValue<Byte8> pcmpeqb(RValue<Byte8> x, RValue<Byte8> y)
{
	return As<Byte8>(V(lowerPCMP(llvm::ICmpInst::ICMP_EQ, V(x.value()), V(y.value()), T(Byte8::type()))));
}

RValue<Short8> packssdw(RValue<Int4> x, RValue<Int4> y)
{
	return RValue<Short8>(createInstruction(llvm::Intrinsic::x86_sse2_packssdw_128, x.value(), y.value()));
}

RValue<SByte8> packsswb(RValue<Short4> x, RValue<Short4> y)
{
	return As<SByte8>(createInstruction(llvm::Intrinsic::x86_sse2_packsswb_128, x.value(), y.value()));
}

RValue<Byte8> packuswb(RValue<Short4> x, RValue<Short4> y)
{
	return As<Byte8>(createInstruction(llvm::Intrinsic::x86_sse2_packuswb_128, x.value(), y.value()));
}

RValue<UShort8> packusdw(RValue<Int4> x, RValue<Int4> y)
{
	if(hasSSE41)
	{
		return RValue<UShort8>(createInstruction(llvm::Intrinsic::x86_sse41_packusdw, x.value(), y.value()));
	}
	else
	{
		RValue<Int4> bx = (x & ~(x >> 31)) - Int4(0x8000);
		RValue<Int4> by = (y & ~(y >> 31)) - Int4(0x8000);

		return As<UShort8>(packssdw(bx, by) + Short8(0x8000u));
	}
}

RValue<UShort4> psrlw(RValue<UShort4> x, unsigned char y)
{
	return As<UShort4>(createInstruction(llvm::Intrinsic::x86_sse2_psrli_w, x.value(), Nucleus::createConstantInt(y)));
}

RValue<Short4> psraw(RValue<Short4> x, unsigned char y)
{
	return As<Short4>(createInstruction(llvm::Intrinsic::x86_sse2_psrai_w, x.value(), Nucleus::createConstantInt(y)));
}

RValue<Short4> psllw(RValue<Short4> x, unsigned char y)
{
	return As<Short4>(createInstruction(llvm::Intrinsic::x86_sse2_pslli_w, x.value(), Nucleus::createConstantInt(y)));
}

RValue<Int4> pslld(RValue<Int4> x, unsigned char y)
{
	return RValue<Int4>(createInstruction(llvm::Intrinsic::x86_sse2_pslli_d, x.value(), Nucleus::createConstantInt(y)));
}

RValue<Int4> psrad(RValue<Int4> x, unsigned char y)
{
	return RValue<Int4>(createInstruction(llvm::Intrinsic::x86_sse2_psrai_d, x.value(), Nucleus::createConstantInt(y)));
}

RValue<UInt4> psrld(RValue<UInt4> x, unsigned char y)
{
	return RValue<UInt4>(createInstruction(llvm::Intrinsic::x86_sse2_psrli_d, x.value(), Nucleus::createConstantInt(y)));
}

RValue<Int4> pmaxsd(RValue<Int4> x, RValue<Int4> y)
{
	return RValue<Int4>(V(lowerPMINMAX(V(x.value()), V(y.value()), llvm::ICmpInst::ICMP_SGT)));
}

RValue<Int4> pminsd(RValue<Int4> x, RValue<Int4> y)
{
	return RValue<Int4>(V(lowerPMINMAX(V(x.value()), V(y.value()), llvm::ICmpInst::ICMP_SLT)));
}

RValue<UInt4> pminud(RValue<UInt4> x, RValue<UInt4> y)
{
	return RValue<UInt4>(V(lowerPMINMAX(V(x.value()), V(y.value()), llvm::ICmpInst::ICMP_ULT)));
}

RValue<Short4> pmulhw(RValue<Short4> x, RValue<Short4> y)
{
	return As<Short4>(createInstruction(llvm::Intrinsic::x86_sse2_pmulh_w, x.value(), y.value()));
}

RValue<UShort4> pmulhuw(RValue<UShort4> x, RValue<UShort4> y)
{
	return As<UShort4>(createInstruction(llvm::Intrinsic::x86_sse2_pmulhu_w, x.value(), y.value()));
}

RValue<Int2> pmaddwd(RValue<Short4> x, RValue<Short4> y)
{
	return As<Int2>(createInstruction(llvm::Intrinsic::x86_sse2_pmadd_wd, x.value(), y.value()));
}

RValue<Int> movmskps(RValue<Float4> x)
{
	return RValue<Int>(createInstruction(llvm::Intrinsic::x86_sse_movmsk_ps, x.value()));
}

RValue<Int> pmovmskb(RValue<Byte8> x)
{
	return RValue<Int>(createInstruction(llvm::Intrinsic::x86_sse2_pmovmskb_128, x.value())) & 0xFF;
}

RValue<Int4> pmovzxbd(RValue<Byte16> x)
{
	return RValue<Int4>(V(lowerPMOV(V(x.value()), T(Int4::type()), false)));
}

RValue<Int4> pmovsxbd(RValue<SByte16> x)
{
	return RValue<Int4>(V(lowerPMOV(V(x.value()), T(Int4::type()), true)));
}

RValue<Int4> pmovzxwd(RValue<UShort8> x)
{
	return RValue<Int4>(V(lowerPMOV(V(x.value()), T(Int4::type()), false)));
}

RValue<Int4> pmovsxwd(RValue<Short8> x)
{
	return RValue<Int4>(V(lowerPMOV(V(x.value()), T(Int4::type()), true)));
}

}
#endif

#ifdef ENABLE_RR_PRINT
void VPrintf(const std::vector<Value *> &vals)
{
	auto i32Ty = ::llvm::Type::getInt32Ty(jit->context);
	auto i8PtrTy = ::llvm::Type::getInt8PtrTy(jit->context);
	auto funcTy = ::llvm::FunctionType::get(i32Ty, { i8PtrTy }, true);
	auto func = jit->module->getOrInsertFunction("rr::DebugPrintf", funcTy);
	jit->builder->CreateCall(func, V(vals));
}
#endif

void Nop()
{
	auto voidTy = ::llvm::Type::getVoidTy(jit->context);
	auto funcTy = ::llvm::FunctionType::get(voidTy, {}, false);
	auto func = jit->module->getOrInsertFunction("nop", funcTy);
	jit->builder->CreateCall(func);
}

void EmitDebugLocation()
{
#ifdef ENABLE_RR_DEBUG_INFO
	if(jit->debugInfo != nullptr)
	{
		jit->debugInfo->EmitLocation();
	}
#endif
}

void EmitDebugVariable(Value *value)
{
#ifdef ENABLE_RR_DEBUG_INFO
	if(jit->debugInfo != nullptr)
	{
		jit->debugInfo->EmitVariable(value);
	}
#endif
}

void FlushDebug()
{
#ifdef ENABLE_RR_DEBUG_INFO
	if(jit->debugInfo != nullptr)
	{
		jit->debugInfo->Flush();
	}
#endif
}

}
