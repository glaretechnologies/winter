/*=====================================================================
Diagnostics.cpp
-------------------
Copyright Nicholas Chapman
Generated at 2011-06-11 19:54:39 +0100
=====================================================================*/
#include "wnt_Diagnostics.h"


#include <utils/StringUtils.h>


namespace Winter
{


namespace Diagnostics
{


const std::string positionString(const BufferPosition& pos)
{
	if(pos.buffer.isNull())
		return "\nPosition unknown";

	size_t line, col;
	StringUtils::getPosition(pos.buffer->source, pos.pos, line, col);

	// Get entire line
	std::string linestr = StringUtils::getLineFromBuffer(
		pos.buffer->source,
		pos.pos - col
	);

	
	std::string s = "\n";
	// Print something like 'somefile.win, line 5:'
	// Note that line is zero-based, so convert to 1-based line number 
	s += pos.buffer->name + ", line " + toString(line + 1) + ":\n";
	s += linestr + "\n";
	col = col < (unsigned int)linestr.size() ? col : (unsigned int)linestr.size();

	// Draw a pointer under the error location, and a squiggly line to the right for the remaining chars in the token.
	for(unsigned int i=0; i<col; ++i)
	{
		// If the source line has a tab, we need to use a tab for spacing as well, to match the position correctly.
		if(linestr[i] == '\t')
			s += "\t";
		else
			s += " ";
	}
	
	s += "^";
	for(size_t z=1; z<pos.len; ++z)
		s += "~";

	return s;
}


} // end namespace Diagnostics


} // end namespace Winter

