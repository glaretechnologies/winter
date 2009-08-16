//Copyright 2009 Nicholas Chapman
#include "Linker.h"


namespace Winter
{


Linker::Linker()
{}


Linker::~Linker()
{}


void Linker::addFunctions(BufferRoot& root)
{
	for(unsigned int i=0; i<root.func_defs.size(); ++i)
	{
		Reference<FunctionDefinition> def = root.func_defs[i];

		this->functions.insert(std::make_pair(def->sig, def));
	}
}


void Linker::linkFunctions(BufferRoot& root)
{
	root.linkFunctions(*this);
}

}


