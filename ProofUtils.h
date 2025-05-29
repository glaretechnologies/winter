#pragma once


#include "wnt_ASTNode.h"
#include "../maths/vec2.h"
#include "../maths/IntervalSet.h"
#include <vector>


namespace Winter
{

class ValueAllocator;


class ProofUtils
{
public:

	static IntervalSetInt64 getInt64Range(std::vector<ASTNode*>& stack, const ASTNodeRef& integer_value, const Reference<ValueAllocator>& value_allocator);
	static IntervalSetFloat getFloatRange(std::vector<ASTNode*>& stack, const ASTNodeRef& integer_value, const Reference<ValueAllocator>& value_allocator);
};


} // end namespace Winter
