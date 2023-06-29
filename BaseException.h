/*=====================================================================
BaseException.h
---------------
Copyright Glare Technologies Limited 2019 -
=====================================================================*/
#pragma once


#include "wnt_Diagnostics.h"
#include <utils/Exception.h>


namespace Winter
{


class BaseException : public glare::Exception
{
public:
	BaseException(const std::string& s) : glare::Exception(s) {}
	virtual ~BaseException() {}
	virtual std::string messageWithPosition() { return what(); } // Exception message, with source position info if there is some.
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
