/*=====================================================================
Diagnostics.cpp
-------------------
Copyright Nicholas Chapman
Generated at 2011-06-11 19:54:39 +0100
=====================================================================*/
#include "wnt_Diagnostics.h"


#include <utils/stringutils.h>


namespace Winter
{


namespace Diagnostics
{


const std::string positionString(const SourceBuffer& source_buffer, uint32 char_index)
{
	unsigned int line, col;
	StringUtils::getPosition(source_buffer.source, char_index, line, col);

	// Get entire line
	std::string linestr = StringUtils::getLineFromBuffer(
		source_buffer.source, 
		char_index - col
	);

	
	std::string s = "\n";
	// Print something like 'somefile.win, line 5:'
	// Note that line is zero-based, so convert to 1-based line number 
	s += source_buffer.name + ", line " + toString(line + 1) + ":\n";
	s += linestr + "\n";
	for(unsigned int i=0; i<col; ++i)
		s += "-";

	s += "^";

	return s;
}


} // end namespace Diagnostics


} // end namespace Winter

