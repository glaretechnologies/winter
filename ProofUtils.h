#pragma once


#include "wnt_ASTNode.h"
#include "../maths/vec2.h"
#include "../maths/IntervalSet.h"
#include <vector>


namespace Winter
{


class ProofUtils
{
public:

	static IntervalSetInt64 getInt64Range(std::vector<ASTNode*>& stack, const ASTNodeRef& integer_value);
	static IntervalSetFloat getFloatRange(std::vector<ASTNode*>& stack, const ASTNodeRef& integer_value);
};


} // end namespace Winter
