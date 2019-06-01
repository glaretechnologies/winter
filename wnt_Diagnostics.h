/*=====================================================================
Diagnostics.h
-------------------
Copyright Nicholas Chapman
Generated at 2011-06-11 19:54:39 +0100
=====================================================================*/
#pragma once


#include "wnt_SourceBuffer.h"
#include <string>
#include <utils/Platform.h>


namespace Winter
{


class BufferPosition
{
public:
	BufferPosition(const SourceBufferConstRef& buffer_, size_t pos_, size_t len_) : buffer(buffer_), pos(pos_), len(len_) {}

	SourceBufferConstRef buffer;
	size_t pos;
	size_t len;
};


/*=====================================================================
Diagnostics
-------------------

=====================================================================*/
namespace Diagnostics
{
	const std::string positionString(const BufferPosition& pos);

};


} // end namespace Winter

