/*=====================================================================
BaseException.h
---------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#pragma once


#include "wnt_Diagnostics.h"


namespace Winter
{


class BaseException
{
public:
	BaseException(const std::string& s_) : s(s_) {}
	virtual ~BaseException() {}
	virtual const std::string what() const { return s; }
private:
	std::string s;
};



class ExceptionWithPosition : public BaseException
{
public:
	ExceptionWithPosition(const std::string& s_, const BufferPosition& pos_) : BaseException(s_), _pos(pos_) {}
	const BufferPosition& pos() const { return _pos; }
private:
	BufferPosition _pos;
};


}
