/*=====================================================================
BaseException.h
---------------
Copyright Glare Technologies Limited 2019 -
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
	virtual std::string what() const { return s; } // Get exception message with source position info.
	virtual std::string messageWithPosition() { return s; } // Exception message, with source position info if there is some.
private:
	std::string s;
};



class ExceptionWithPosition : public BaseException
{
public:
	ExceptionWithPosition(const std::string& s_, const BufferPosition& pos_) : BaseException(s_), _pos(pos_) {}
	const BufferPosition& pos() const { return _pos; }

	virtual std::string messageWithPosition()
	{
		return what() + Diagnostics::positionString(_pos);
	}
private:
	BufferPosition _pos;
};


}
