/*=====================================================================
TokenBase.h
-----------
File created by ClassTemplate on Wed Oct 22 14:51:44 2008
Code By Nicholas Chapman.

Copyright 2009 Nicholas Chapman
=====================================================================*/
#ifndef __TOKENBASE_H_666_
#define __TOKENBASE_H_666_


#include "utils/refcounted.h"
#include "utils/reference.h"
#include <string>
//#include "Type.h"


namespace Winter
{


class TokenBaseExcep
{
public:
	TokenBaseExcep(const std::string& text_) : text(text_) {}
	~TokenBaseExcep(){}
	const std::string& what() const { return text; }
private:
	std::string text;
};


const unsigned int FLOAT_LITERAL_TOKEN = 0;
const unsigned int INT_LITERAL_TOKEN = 1;
const unsigned int BOOL_LITERAL_TOKEN = 2;
const unsigned int STRING_LITERAL_TOKEN = 3;
const unsigned int IDENTIFIER_TOKEN = 4;


class TokenBase : public RefCounted
{
public:
	TokenBase(unsigned int char_index_, unsigned int type_) : char_index(char_index_), type(type_) {}
	virtual ~TokenBase(){}

	inline unsigned int getType() const { return type; }
	inline bool isIdentifier() const { return type == IDENTIFIER_TOKEN; }
	
	virtual bool isLiteral() const = 0;
	virtual bool isParanthesis() const = 0;
	virtual bool isBinaryInfixOp() const = 0;
	
	virtual const std::string functionName() const { throw TokenBaseExcep("functionName()"); }
	virtual int precedence() const { throw TokenBaseExcep("precedence()"); }

	virtual const std::string& getIdentifierValue() const { throw TokenBaseExcep("getIdentifierValue()"); }
	virtual float getFloatLiteralValue() const { throw TokenBaseExcep("getRealLiteralValue()"); }
	virtual int getIntLiteralValue() const { throw TokenBaseExcep("getIntLiteralValue()"); }
	virtual bool getBoolLiteralValue() const { throw TokenBaseExcep("getBoolLiteralValue()"); }
	virtual const std::string& getStringLiteralValue() const { throw TokenBaseExcep("getStringLiteralValue()"); }

	unsigned int char_index;
private:
	unsigned int type;
};


class IdentifierToken : public TokenBase
{
public:
	IdentifierToken(const std::string& x, unsigned int char_index) : TokenBase(char_index, IDENTIFIER_TOKEN), val(x) {}
	
	virtual bool isLiteral() const { return false; }
	virtual bool isParanthesis() const { return false; }
	virtual bool isBinaryInfixOp() const { return false; }

	virtual const std::string& getIdentifierValue() const { return val; }

private:
	std::string val;
};


class FloatLiteralToken : public TokenBase
{
public:
	FloatLiteralToken(float x, unsigned int char_index) : TokenBase(char_index, FLOAT_LITERAL_TOKEN), val(x) {}
	
	virtual bool isLiteral() const { return true; }
	virtual bool isParanthesis() const { return false; }
	virtual bool isBinaryInfixOp() const { return false; }

	virtual float getFloatLiteralValue() const { return val; }
private:
	float val;
};


class IntLiteralToken : public TokenBase
{
public:
	IntLiteralToken(int x, unsigned int char_index) : TokenBase(char_index, INT_LITERAL_TOKEN), val(x) {}
	
	virtual bool isLiteral() const { return true; }
	virtual bool isParanthesis() const { return false; }
	virtual bool isBinaryInfixOp() const { return false; }

	virtual int getIntLiteralValue() const { return val; }
private:
	int val;
};


class BoolLiteralToken : public TokenBase
{
public:
	BoolLiteralToken(bool x, unsigned int char_index) : TokenBase(char_index, BOOL_LITERAL_TOKEN), val(x) {}
	
	virtual bool isLiteral() const { return true; }
	virtual bool isParanthesis() const { return false; }
	virtual bool isBinaryInfixOp() const { return false; }

	virtual bool getBoolLiteralValue() const { return val; }
private:
	bool val;
};


class StringLiteralToken : public TokenBase
{
public:
	StringLiteralToken(const std::string& x, unsigned int char_index) : TokenBase(char_index, STRING_LITERAL_TOKEN), val(x) {}
	
	virtual bool isLiteral() const { return true; }
	virtual bool isParanthesis() const { return false; }
	virtual bool isBinaryInfixOp() const { return false; }

	virtual const std::string& getStringLiteralValue() const { return val; }
private:
	std::string val;
};


#include "GeneratedTokens.h"


} //end namespace Lang


#endif //__TOKENBASE_H_666_
