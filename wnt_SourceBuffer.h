/*=====================================================================
SourceBuffer.h
-------------------
Copyright Nicholas Chapman
Generated at 2011-06-11 19:19:16 +0100
=====================================================================*/
#pragma once


#include <string>
#include <utils/Reference.h>
#include <utils/RefCounted.h>


namespace Winter
{


/*=====================================================================
SourceBuffer
-------------------

=====================================================================*/
class SourceBuffer : public RefCounted
{
public:
	SourceBuffer(const std::string& name_, const std::string& source_) : name(name_), source(source_) {}
	~SourceBuffer();

	static Reference<SourceBuffer> loadFromDisk(const std::string& path); // Throw BaseException on failure

	std::string name;
	std::string source;
private:

};


typedef Reference<SourceBuffer> SourceBufferRef;


} // end namespace Winter

