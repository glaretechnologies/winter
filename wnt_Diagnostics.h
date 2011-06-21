/*=====================================================================
Diagnostics.h
-------------------
Copyright Nicholas Chapman
Generated at 2011-06-11 19:54:39 +0100
=====================================================================*/
#pragma once


#include "wnt_SourceBuffer.h"
#include <string>
#include <utils/platform.h>


namespace Winter
{


/*=====================================================================
Diagnostics
-------------------

=====================================================================*/
namespace Diagnostics
{
	const std::string positionString(const SourceBuffer& source_buffer, uint32 char_index);

};


} // end namespace Winter

