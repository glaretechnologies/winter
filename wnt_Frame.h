#pragma once


//#include "wnt_ASTNode.h"
#include <utils/RefCounted.h>
#include <vector>


namespace Winter
{


class NamedConstant;


class Frame : public RefCounted
{
public:
	typedef std::map<std::string, std::vector<Reference<FunctionDefinition> > > NameToFuncMapType;
	NameToFuncMapType name_to_functions_map;

	typedef std::map<std::string, Reference<NamedConstant> > NamedConstantMap;
	NamedConstantMap named_constant_map;
};


typedef Reference<Frame> FrameRef;


} // end namespace Winter
