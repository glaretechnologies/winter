#include "Type.h"


#include "../../indigosvn/trunk/utils/stringutils.h"


namespace Winter
{


const std::string Function::toString() const // { return "function"; 
{
	std::string s = "function<";
	std::vector<std::string> typestrings;
	for(unsigned int i=0;i<arg_types.size(); ++i)
		typestrings.push_back(arg_types[i]->toString());
	
	typestrings.push_back(this->return_type->toString());

	//s += StringUtils::join(typestrings, ", ");

	//s += ", " + this->return_type->toString();
	s += StringUtils::join(typestrings, ", ");
	return s + ">";
}


/*const std::string StructureType::toString() const 
{ 
	std::string s = "struct<";
	std::vector<std::string> typestrings;
	for(unsigned int i=0;i<arg_types.size(); ++i)
		typestrings.push_back(arg_types[i]->toString());
	
	typestrings.push_back(this->return_type->toString());

	//s += StringUtils::join(typestrings, ", ");

	//s += ", " + this->return_type->toString();
	s += StringUtils::join(typestrings, ", ");
	return s + ">";
}*/


} // end namespace Winter
