#pragma once


//#include "wnt_ASTNode.h"
#include <utils/refcounted.h>
#include <vector>


namespace Winter
{


class Frame : public RefCounted
{
public:
	typedef std::map<std::string, std::vector<Reference<FunctionDefinition> > > NameToFuncMapType;
	NameToFuncMapType name_to_functions_map;
};


typedef Reference<Frame> FrameRef;


} // end namespace Winter
