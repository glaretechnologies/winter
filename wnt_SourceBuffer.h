/*=====================================================================
SourceBuffer.h
-------------------
Copyright Nicholas Chapman
Generated at 2011-06-11 19:19:16 +0100
=====================================================================*/
#pragma once


#include <utils/Reference.h>
#include <utils/RefCounted.h>
#include <string>
#include <map>


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

	std::map<std::string, std::string> user_data; // Stringly-typed user data.
private:

};


typedef Reference<SourceBuffer> SourceBufferRef;
typedef Reference<const SourceBuffer> SourceBufferConstRef;


class SrcLocation
{
public:
	SrcLocation(size_t char_index_, size_t len_, const SourceBuffer* buf) :
		char_index(char_index_), len(len_), source_buffer(buf) {}

	static const SrcLocation invalidLocation() { return SrcLocation(4000000000u, 0, NULL); }

	bool isValid() const { return char_index != 4000000000u; }

	size_t char_index;
	size_t len;
	const SourceBuffer* source_buffer;
};



} // end namespace Winter

