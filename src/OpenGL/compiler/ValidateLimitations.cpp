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

#include "ValidateLimitations.h"

#include "debug.h"
#include "InfoSink.h"
#include "InitializeParseContext.h"
#include "ParseHelper.h"

#include <cstring>

#define GL_VERTEX_SHADER 0x8B31

namespace {

bool IsLoopIndex(const TIntermSymbol* symbol, const TLoopStack& stack)
{
	for(TLoopStack::const_iterator i = stack.begin(); i != stack.end(); ++i)
	{
		if(i->index.id == symbol->getId())
			return true;
	}

	return false;
}

void MarkLoopForUnroll(const TIntermSymbol* symbol, TLoopStack& stack)
{
	for(TLoopStack::iterator i = stack.begin(); i != stack.end(); ++i)
	{
		if(i->index.id == symbol->getId())
		{
			ASSERT(i->loop);
			i->loop->setUnrollFlag(true);
			return;
		}
	}

	UNREACHABLE(0);
}

//
// Traverses a node to check if it represents a constant index expression.
//
class ValidateConstIndexExpr : public TIntermTraverser
{
public:
	ValidateConstIndexExpr(const TLoopStack& stack) :
		mValid(true), mLoopStack(stack) {}

	// Returns true if the parsed node represents a constant index expression.
	bool isValid() const { return mValid; }

	virtual void visitSymbol(TIntermSymbol* symbol) {
		// Only constants and loop indices are allowed in a constant index expression.
		if(mValid)
		{
			mValid = (symbol->getQualifier() == EvqConstExpr) ||
			         IsLoopIndex(symbol, mLoopStack);
		}
	}

private:
	bool mValid;
	const TLoopStack& mLoopStack;
};

//
// Traverses a node to check if it uses a loop index.
// If an int loop index is used in its body as a sampler array index,
// mark the loop for unroll.
//
class ValidateLoopIndexExpr : public TIntermTraverser
{
public:
	ValidateLoopIndexExpr(TLoopStack& stack) :
		mUsesFloatLoopIndex(false),
	  mUsesIntLoopIndex(false),
	  mLoopStack(stack) {}

	bool usesFloatLoopIndex() const { return mUsesFloatLoopIndex; }
	bool usesIntLoopIndex() const { return mUsesIntLoopIndex; }

	virtual void visitSymbol(TIntermSymbol* symbol)
	{
		if(IsLoopIndex(symbol, mLoopStack))
		{
			switch(symbol->getBasicType())
			{
			case EbtFloat:
				mUsesFloatLoopIndex = true;
				break;
			case EbtUInt:
				mUsesIntLoopIndex = true;
				MarkLoopForUnroll(symbol, mLoopStack);
				break;
			case EbtInt:
				mUsesIntLoopIndex = true;
				MarkLoopForUnroll(symbol, mLoopStack);
				break;
			default:
				UNREACHABLE(symbol->getBasicType());
				break;
			}
		}
	}

private:
	bool mUsesFloatLoopIndex;
	bool mUsesIntLoopIndex;
	TLoopStack& mLoopStack;
};

}

bool ValidateLimitations::visitBinary(Visit, TIntermBinary* node)
{
	// Check if loop index is modified in the loop body.
	validateOperation(node, node->getLeft());

	// Check indexing.
	switch(node->getOp())
	{
	case EOpIndexDirect:
		validateIndexing(node);
		break;
	case EOpIndexIndirect:
		validateIndexing(node);
		break;
	default: break;
	}

	return true;
}

bool ValidateLimitations::visitUnary(Visit, TIntermUnary* node)
{
	// Check if loop index is modified in the loop body.
	validateOperation(node, node->getOperand());

	return true;
}

bool ValidateLimitations::visitAggregate(Visit, TIntermAggregate* node)
{
	switch(node->getOp())
	{
	case EOpFunctionCall:
		validateFunctionCall(node);
		break;
	default:
		break;
	}

	return true;
}

bool ValidateLimitations::visitLoop(Visit, TIntermLoop* node)
{
	if(!validateLoopType(node))
		return false;

	TLoopInfo info;
	memset(&info, 0, sizeof(TLoopInfo));
	info.loop = node;
	if(!validateForLoopHeader(node, &info))
		return false;

	TIntermNode* body = node->getBody();
	if(body)
	{
		mLoopStack.push_back(info);
		body->traverse(this);
		mLoopStack.pop_back();
	}

	// The loop is fully processed, no need to visit children.
	return false;
}

void ValidateLimitations::error(TSourceLoc loc, const char *reason, const char* token)
{
	mSink.prefix(EPrefixError);
	mSink.location(loc);
	mSink << "'" << token << "' : " << reason << "\n";
	++mNumErrors;
}

bool ValidateLimitations::withinLoopBody() const
{
	return !mLoopStack.empty();
}

bool ValidateLimitations::isLoopIndex(const TIntermSymbol* symbol) const
{
	return IsLoopIndex(symbol, mLoopStack);
}

bool ValidateLimitations::validateLoopType(TIntermLoop* node)
{
	TLoopType type = node->getType();
	if(type == ELoopFor)
		return true;

	// Reject while and do-while loops.
	error(node->getLine(), "This type of loop is not allowed", type == ELoopWhile ? "while" : "do");
	return false;
}

bool ValidateLimitations::validateForLoopHeader(TIntermLoop* node, TLoopInfo* info)
{
	ASSERT(node->getType() == ELoopFor);

	// The for statement has the form:
	//     for(init-declaration; condition; expression) statement
	if(!validateForLoopInit(node, info))
		return false;
	if(!validateForLoopCond(node, info))
		return false;
	if(!validateForLoopExpr(node, info))
		return false;

	return true;
}

bool ValidateLimitations::validateForLoopInit(TIntermLoop* node, TLoopInfo* info)
{
	TIntermNode* init = node->getInit();
	if(!init)
	{
		error(node->getLine(), "Missing init declaration", "for");
		return false;
	}

	// init-declaration has the form:
	//     type-specifier identifier = constant-expression
	TIntermAggregate* decl = init->getAsAggregate();
	if(!decl || (decl->getOp() != EOpDeclaration))
	{
		error(init->getLine(), "Invalid init declaration", "for");
		return false;
	}

	// To keep things simple, do not allow declaration list.
	TIntermSequence& declSeq = decl->getSequence();
	if(declSeq.size() != 1)
	{
		error(decl->getLine(), "Invalid init declaration", "for");
		return false;
	}

	TIntermBinary* declInit = declSeq[0]->getAsBinaryNode();
	if(!declInit || (declInit->getOp() != EOpInitialize))
	{
		error(decl->getLine(), "Invalid init declaration", "for");
		return false;
	}

	TIntermSymbol* symbol = declInit->getLeft()->getAsSymbolNode();
	if(!symbol)
	{
		error(declInit->getLine(), "Invalid init declaration", "for");
		return false;
	}

	// The loop index has type int or float.
	TBasicType type = symbol->getBasicType();
	if(!IsInteger(type) && (type != EbtFloat))
	{
		error(symbol->getLine(), "Invalid type for loop index", getBasicString(type));
		return false;
	}

	// The loop index is initialized with constant expression.
	if(!isConstExpr(declInit->getRight()))
	{
		error(declInit->getLine(), "Loop index cannot be initialized with non-constant expression", symbol->getSymbol().c_str());
		return false;
	}

	info->index.id = symbol->getId();

	return true;
}

bool ValidateLimitations::validateForLoopCond(TIntermLoop* node, TLoopInfo* info)
{
	TIntermNode* cond = node->getCondition();
	if(!cond)
	{
		error(node->getLine(), "Missing condition", "for");
		return false;
	}

	// condition has the form:
	//     loop_index relational_operator constant_expression
	TIntermBinary* binOp = cond->getAsBinaryNode();
	if(!binOp)
	{
		error(node->getLine(), "Invalid condition", "for");
		return false;
	}

	// Loop index should be to the left of relational operator.
	TIntermSymbol* symbol = binOp->getLeft()->getAsSymbolNode();
	if(!symbol)
	{
		error(binOp->getLine(), "Invalid condition", "for");
		return false;
	}

	if(symbol->getId() != info->index.id)
	{
		error(symbol->getLine(), "Expected loop index", symbol->getSymbol().c_str());
		return false;
	}

	// Relational operator is one of: > >= < <= == !=
	switch(binOp->getOp())
	{
	case EOpEqual:
	case EOpNotEqual:
	case EOpLessThan:
	case EOpGreaterThan:
	case EOpLessThanEqual:
	case EOpGreaterThanEqual:
		break;
	default:
		error(binOp->getLine(), "Invalid relational operator", getOperatorString(binOp->getOp()));
		break;
	}

	// Loop index must be compared with a constant.
	if(!isConstExpr(binOp->getRight()))
	{
		error(binOp->getLine(), "Loop index cannot be compared with non-constant expression", symbol->getSymbol().c_str());
		return false;
	}

	return true;
}

bool ValidateLimitations::validateForLoopExpr(TIntermLoop* node, TLoopInfo* info)
{
	TIntermNode* expr = node->getExpression();
	if(!expr)
	{
		error(node->getLine(), "Missing expression", "for");
		return false;
	}

	// for expression has one of the following forms:
	//     loop_index++
	//     loop_index--
	//     loop_index += constant_expression
	//     loop_index -= constant_expression
	//     ++loop_index
	//     --loop_index
	TIntermUnary* unOp = expr->getAsUnaryNode();
	TIntermBinary* binOp = unOp ? nullptr : expr->getAsBinaryNode();

	TOperator op = EOpNull;
	TIntermSymbol* symbol = nullptr;
	if(unOp)
	{
		op = unOp->getOp();
		symbol = unOp->getOperand()->getAsSymbolNode();
	}
	else if(binOp)
	{
		op = binOp->getOp();
		symbol = binOp->getLeft()->getAsSymbolNode();
	}

	// The operand must be loop index.
	if(!symbol)
	{
		error(expr->getLine(), "Invalid expression", "for");
		return false;
	}
	if(symbol->getId() != info->index.id)
	{
		error(symbol->getLine(), "Expected loop index", symbol->getSymbol().c_str());
		return false;
	}

	// The operator is one of: ++ -- += -=
	switch(op)
	{
		case EOpPostIncrement:
		case EOpPostDecrement:
		case EOpPreIncrement:
		case EOpPreDecrement:
			ASSERT((unOp != NULL) && (binOp == NULL));
			break;
		case EOpAddAssign:
		case EOpSubAssign:
			ASSERT((unOp == NULL) && (binOp != NULL));
			break;
		default:
			error(expr->getLine(), "Invalid operator", getOperatorString(op));
			return false;
	}

	// Loop index must be incremented/decremented with a constant.
	if(binOp != NULL)
	{
		if(!isConstExpr(binOp->getRight()))
		{
			error(binOp->getLine(),
				  "Loop index cannot be modified by non-constant expression",
				  symbol->getSymbol().c_str());
			return false;
		}
	}

	return true;
}

bool ValidateLimitations::validateFunctionCall(TIntermAggregate* node)
{
	ASSERT(node->getOp() == EOpFunctionCall);

	// If not within loop body, there is nothing to check.
	if(!withinLoopBody())
		return true;

	// List of param indices for which loop indices are used as argument.
	typedef std::vector<int> ParamIndex;
	ParamIndex pIndex;
	TIntermSequence& params = node->getSequence();
	for(TIntermSequence::size_type i = 0; i < params.size(); ++i)
	{
		TIntermSymbol* symbol = params[i]->getAsSymbolNode();
		if(symbol && isLoopIndex(symbol))
			pIndex.push_back(i);
	}
	// If none of the loop indices are used as arguments,
	// there is nothing to check.
	if(pIndex.empty())
		return true;

	bool valid = true;
	TSymbolTable& symbolTable = GetGlobalParseContext()->symbolTable;
	TSymbol* symbol = symbolTable.find(node->getName(), GetGlobalParseContext()->getShaderVersion());
	ASSERT(symbol && symbol->isFunction());
	TFunction* function = static_cast<TFunction*>(symbol);
	for(ParamIndex::const_iterator i = pIndex.begin(); i != pIndex.end(); ++i)
	{
		const TParameter& param = function->getParam(*i);
		TQualifier qual = param.type->getQualifier();
		if((qual == EvqOut) || (qual == EvqInOut))
		{
			error(params[*i]->getLine(), "Loop index cannot be used as argument to a function out or inout parameter", params[*i]->getAsSymbolNode()->getSymbol().c_str());
			valid = false;
		}
	}

	return valid;
}

bool ValidateLimitations::validateOperation(TIntermOperator* node, TIntermNode* operand)
{
	// Check if loop index is modified in the loop body.
	if(!withinLoopBody() || !node->modifiesState())
		return true;

	const TIntermSymbol* symbol = operand->getAsSymbolNode();
	if(symbol && isLoopIndex(symbol))
	{
		error(node->getLine(), "Loop index cannot be statically assigned to within the body of the loop", symbol->getSymbol().c_str());
	}

	return true;
}

bool ValidateLimitations::isConstExpr(TIntermNode* node)
{
	ASSERT(node);

	return node->getAsConstantUnion() != nullptr;
}

bool ValidateLimitations::isConstIndexExpr(TIntermNode* node)
{
	ASSERT(node);

	ValidateConstIndexExpr validate(mLoopStack);
	node->traverse(&validate);
	return validate.isValid();
}

bool ValidateLimitations::validateIndexing(TIntermBinary* node)
{
	ASSERT((node->getOp() == EOpIndexDirect) || (node->getOp() == EOpIndexIndirect));

	bool valid = true;
	TIntermTyped* index = node->getRight();
	// The index expression must have integral type.
	if(!index->isScalarInt())
	{
		error(index->getLine(), "Index expression must have integral type", index->getCompleteString().c_str());
		valid = false;
	}

	// The index expession must be a constant-index-expression unless
	// the operand is a uniform in a vertex shader.
	TIntermTyped* operand = node->getLeft();
	bool skip = (mShaderType == GL_VERTEX_SHADER) && (operand->getQualifier() == EvqUniform);
	if(!skip && !isConstIndexExpr(index))
	{
		error(index->getLine(), "Index expression must be constant", "[]");
		valid = false;
	}

	return valid;
}
