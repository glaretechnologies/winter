/*=====================================================================
SourceBuffer.cpp
-------------------
Copyright Nicholas Chapman
Generated at 2011-06-11 19:19:16 +0100
=====================================================================*/
#include "wnt_SourceBuffer.h"


#include "BaseException.h"
#include <utils/FileUtils.h>


namespace Winter
{


/*SourceBuffer::SourceBuffer()
{

}*/


SourceBuffer::~SourceBuffer()
{

}


SourceBufferRef SourceBuffer::loadFromDisk(const std::string& path) // Throw BaseException on failure
{
	try
	{
		std::string file_contents;
		FileUtils::readEntireFile(path, file_contents);
		return Winter::SourceBufferRef(new Winter::SourceBuffer(path, file_contents));
	}
	catch(FileUtils::FileUtilsExcep& e)
	{
		throw Winter::BaseException(e.what());
	}
}


} // end namespace Winter

