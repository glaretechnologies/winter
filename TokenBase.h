/*=====================================================================
TokenBase.h
-----------
Copyright Glare Technologies Limited 2016 -
File created by ClassTemplate on Wed Oct 22 14:51:44 2008
=====================================================================*/
#pragma once


#include <utils/Reference.h>
#include <utils/RefCounted.h>
#include <string>


namespace Winter
{


const unsigned int FLOAT_LITERAL_TOKEN = 0;
const unsigned int INT_LITERAL_TOKEN = 1;
const unsigned int BOOL_LITERAL_TOKEN = 2;
const unsigned int STRING_LITERAL_TOKEN = 3;
const unsigned int CHAR_LITERAL_TOKEN = 4;
const unsigned int IDENTIFIER_TOKEN = 5;


class TokenBase : public RefCounted
{
public:
	TokenBase(size_t char_index_, size_t num_chars_, unsigned int type_) : char_index(char_index_), num_chars(num_chars_), type(type_) {}
	virtual ~TokenBase() {}

	inline unsigned int getType() const { return type; }

	inline bool isIdentifier() const { return type == IDENTIFIER_TOKEN; }
	inline bool isLiteral() const { return type <= CHAR_LITERAL_TOKEN; }
	
	inline const std::string& getIdentifierValue() const;
	inline double getFloatLiteralValue() const;
	inline int64 getIntLiteralValue() const;
	inline bool getBoolLiteralValue() const;
	inline const std::string& getStringLiteralValue() const;
	inline const std::string& getCharLiteralValue() const;

	size_t char_index;
	size_t num_chars;
private:
	unsigned int type;
};


class IdentifierToken : public TokenBase
{
public:
	IdentifierToken(const std::string& x, size_t char_index, size_t num_chars) : TokenBase(char_index, num_chars, IDENTIFIER_TOKEN), val(x) {}
	
	std::string val;
};


class FloatLiteralToken : public TokenBase
{
public:
	FloatLiteralToken(double x, char suffix_, size_t char_index, size_t num_chars) : TokenBase(char_index, num_chars, FLOAT_LITERAL_TOKEN), val(x), suffix(suffix_) {}
	
	char suffix; // 'f' or 'd' or 0 if not present.
	double val;
};


class IntLiteralToken : public TokenBase
{
public:
	IntLiteralToken(int64 x, int num_bits_, bool is_signed_, size_t char_index, size_t num_chars) : TokenBase(char_index, num_chars, INT_LITERAL_TOKEN), val(x), num_bits(num_bits_), is_signed(is_signed_) {}
	
	int64 val;
	int num_bits;
	bool is_signed; // Literal is considered signed unless it has a 'u' suffix.
};


class BoolLiteralToken : public TokenBase
{
public:
	BoolLiteralToken(bool x, size_t char_index, size_t num_chars) : TokenBase(char_index, num_chars, BOOL_LITERAL_TOKEN), val(x) {}
	
	bool val;
};


class StringLiteralToken : public TokenBase
{
public:
	StringLiteralToken(const std::string& x, size_t char_index, size_t num_chars) : TokenBase(char_index, num_chars, STRING_LITERAL_TOKEN), val(x) {}
	
	std::string val;
};


class CharLiteralToken : public TokenBase
{
public:
	CharLiteralToken(const std::string& x, size_t char_index, size_t num_chars) : TokenBase(char_index, num_chars, CHAR_LITERAL_TOKEN), val(x) {}
	
	std::string val;
};


//------------------------------------------------------------------------------------


const std::string& TokenBase::getIdentifierValue() const
{
	assert(this->type == IDENTIFIER_TOKEN); 
	return static_cast<const IdentifierToken*>(this)->val;
}


double TokenBase::getFloatLiteralValue() const
{ 
	assert(this->type == FLOAT_LITERAL_TOKEN); 
	return static_cast<const FloatLiteralToken*>(this)->val;
}


int64 TokenBase::getIntLiteralValue() const
{ 
	assert(this->type == INT_LITERAL_TOKEN); 
	return static_cast<const IntLiteralToken*>(this)->val;
}


bool TokenBase::getBoolLiteralValue() const
{ 
	assert(this->type == BOOL_LITERAL_TOKEN); 
	return static_cast<const BoolLiteralToken*>(this)->val;
}


const std::string& TokenBase::getStringLiteralValue() const
{
	assert(this->type == STRING_LITERAL_TOKEN); 
	return static_cast<const StringLiteralToken*>(this)->val;
}


const std::string& TokenBase::getCharLiteralValue() const
{
	assert(this->type == CHAR_LITERAL_TOKEN); 
	return static_cast<const CharLiteralToken*>(this)->val;
}


//------------------------------------------------------------------------------------


const unsigned int COMMA_TOKEN = 10;
class COMMA_Token : public TokenBase
{
public:
	COMMA_Token(size_t char_index) : TokenBase(char_index, /*num_chars=*/1, COMMA_TOKEN) {}
};


const unsigned int OPEN_PARENTHESIS_TOKEN = 11;
class OPEN_PARENTHESIS_Token : public TokenBase
{
public:
	OPEN_PARENTHESIS_Token(size_t char_index) : TokenBase(char_index, /*num_chars=*/1, OPEN_PARENTHESIS_TOKEN) {}
};


const unsigned int CLOSE_PARENTHESIS_TOKEN = 12;
class CLOSE_PARENTHESIS_Token : public TokenBase
{
public:
	CLOSE_PARENTHESIS_Token(size_t char_index) : TokenBase(char_index, /*num_chars=*/1, CLOSE_PARENTHESIS_TOKEN) {}
};


const unsigned int OPEN_BRACE_TOKEN = 13;
class OPEN_BRACE_Token : public TokenBase
{
public:
	OPEN_BRACE_Token(size_t char_index) : TokenBase(char_index, /*num_chars=*/1, OPEN_BRACE_TOKEN) {}
};


const unsigned int CLOSE_BRACE_TOKEN = 14;
class CLOSE_BRACE_Token : public TokenBase
{
public:
	CLOSE_BRACE_Token(size_t char_index) : TokenBase(char_index, /*num_chars=*/1, CLOSE_BRACE_TOKEN) {}
};


const unsigned int OPEN_SQUARE_BRACKET_TOKEN = 15;
class OPEN_SQUARE_BRACKET_Token : public TokenBase
{
public:
	OPEN_SQUARE_BRACKET_Token(size_t char_index) : TokenBase(char_index, /*num_chars=*/1, OPEN_SQUARE_BRACKET_TOKEN) {}
};


const unsigned int CLOSE_SQUARE_BRACKET_TOKEN = 16;
class CLOSE_SQUARE_BRACKET_Token : public TokenBase
{
public:
	CLOSE_SQUARE_BRACKET_Token(size_t char_index) : TokenBase(char_index, /*num_chars=*/1, CLOSE_SQUARE_BRACKET_TOKEN) {}
	std::string suffix;
};


const unsigned int COLON_TOKEN = 17;
class COLON_Token : public TokenBase
{
public:
	COLON_Token(size_t char_index) : TokenBase(char_index, /*num_chars=*/1, COLON_TOKEN) {}
};


const unsigned int RIGHT_ARROW_TOKEN = 18;
class RIGHT_ARROW_Token : public TokenBase
{
public:
	RIGHT_ARROW_Token(size_t char_index) : TokenBase(char_index, /*num_chars=*/2, RIGHT_ARROW_TOKEN) {}
};


const unsigned int EQUALS_TOKEN = 19;
class EQUALS_Token : public TokenBase
{
public:
	EQUALS_Token(size_t char_index) : TokenBase(char_index, /*num_chars=*/1, EQUALS_TOKEN) {}
};


const unsigned int PLUS_TOKEN = 20;
class PLUS_Token : public TokenBase
{
public:
	PLUS_Token(size_t char_index) : TokenBase(char_index, /*num_chars=*/1, PLUS_TOKEN) {}
};


const unsigned int MINUS_TOKEN = 21;
class MINUS_Token : public TokenBase
{
public:
	MINUS_Token(size_t char_index) : TokenBase(char_index, /*num_chars=*/1, MINUS_TOKEN) {}
};


const unsigned int FORWARDS_SLASH_TOKEN = 22;
class FORWARDS_SLASH_Token : public TokenBase
{
public:
	FORWARDS_SLASH_Token(size_t char_index) : TokenBase(char_index, /*num_chars=*/1, FORWARDS_SLASH_TOKEN) {}
};


const unsigned int BACK_SLASH_TOKEN = 23;
class BACK_SLASH_Token : public TokenBase
{
public:
	BACK_SLASH_Token(size_t char_index) : TokenBase(char_index, /*num_chars=*/1, BACK_SLASH_TOKEN) {}
};


const unsigned int ASTERISK_TOKEN = 24;
class ASTERISK_Token : public TokenBase
{
public:
	ASTERISK_Token(size_t char_index) : TokenBase(char_index, /*num_chars=*/1, ASTERISK_TOKEN) {}
};


const unsigned int LEFT_ANGLE_BRACKET_TOKEN = 25;
class LEFT_ANGLE_BRACKET_Token : public TokenBase
{
public:
	LEFT_ANGLE_BRACKET_Token(size_t char_index) : TokenBase(char_index, /*num_chars=*/1, LEFT_ANGLE_BRACKET_TOKEN) {}
};


const unsigned int RIGHT_ANGLE_BRACKET_TOKEN = 26;
class RIGHT_ANGLE_BRACKET_Token : public TokenBase
{
public:
	RIGHT_ANGLE_BRACKET_Token(size_t char_index) : TokenBase(char_index, /*num_chars=*/1, RIGHT_ANGLE_BRACKET_TOKEN) {}
};


const unsigned int LESS_EQUAL_TOKEN = 27;
class LESS_EQUAL_Token : public TokenBase
{
public:
	LESS_EQUAL_Token(size_t char_index) : TokenBase(char_index, /*num_chars=*/2, LESS_EQUAL_TOKEN) {}
};


const unsigned int GREATER_EQUAL_TOKEN = 28;
class GREATER_EQUAL_Token : public TokenBase
{
public:
	GREATER_EQUAL_Token(size_t char_index) : TokenBase(char_index, /*num_chars=*/2, GREATER_EQUAL_TOKEN) {}
};


const unsigned int DOUBLE_EQUALS_TOKEN = 29;
class DOUBLE_EQUALS_Token : public TokenBase
{
public:
	DOUBLE_EQUALS_Token(size_t char_index) : TokenBase(char_index, /*num_chars=*/2, DOUBLE_EQUALS_TOKEN) {}
};


const unsigned int NOT_EQUALS_TOKEN = 30;
class NOT_EQUALS_Token : public TokenBase
{
public:
	NOT_EQUALS_Token(size_t char_index) : TokenBase(char_index, /*num_chars=*/2, NOT_EQUALS_TOKEN) {}
};


const unsigned int AND_TOKEN = 31;
class AND_Token : public TokenBase
{
public:
	AND_Token(size_t char_index) : TokenBase(char_index, /*num_chars=*/2, AND_TOKEN) {}
};


const unsigned int OR_TOKEN = 32;
class OR_Token : public TokenBase
{
public:
	OR_Token(size_t char_index) : TokenBase(char_index, /*num_chars=*/2, OR_TOKEN) {}
};


const unsigned int EXCLAMATION_MARK_TOKEN = 33;
class EXCLAMATION_MARK_Token : public TokenBase
{
public:
	EXCLAMATION_MARK_Token(size_t char_index) : TokenBase(char_index, /*num_chars=*/1, EXCLAMATION_MARK_TOKEN) {}
};


const unsigned int DOT_TOKEN = 34;
class DOT_Token : public TokenBase
{
public:
	DOT_Token(size_t char_index) : TokenBase(char_index, /*num_chars=*/1, DOT_TOKEN) {}
};


const unsigned int QUESTION_MARK_TOKEN = 35;
class QUESTION_MARK_Token : public TokenBase
{
public:
	QUESTION_MARK_Token(size_t char_index) : TokenBase(char_index, /*num_chars=*/1, QUESTION_MARK_TOKEN) {}
};


const unsigned int BITWISE_AND_TOKEN = 36;
class BITWISE_AND_Token : public TokenBase
{
public:
	BITWISE_AND_Token(size_t char_index) : TokenBase(char_index, /*num_chars=*/1, BITWISE_AND_TOKEN) {}
};


const unsigned int BITWISE_OR_TOKEN = 37;
class BITWISE_OR_Token : public TokenBase
{
public:
	BITWISE_OR_Token(size_t char_index) : TokenBase(char_index, /*num_chars=*/1, BITWISE_OR_TOKEN) {}
};


const unsigned int BITWISE_XOR_TOKEN = 38;
class BITWISE_XOR_Token : public TokenBase
{
public:
	BITWISE_XOR_Token(size_t char_index) : TokenBase(char_index, /*num_chars=*/1, BITWISE_XOR_TOKEN) {}
};


const unsigned int LEFT_SHIFT_TOKEN = 39;
class LEFT_SHIFT_Token : public TokenBase
{
public:
	LEFT_SHIFT_Token(size_t char_index) : TokenBase(char_index, /*num_chars=*/2, LEFT_SHIFT_TOKEN) {}
};


const unsigned int RIGHT_SHIFT_TOKEN = 40;
class RIGHT_SHIFT_Token : public TokenBase
{
public:
	RIGHT_SHIFT_Token(size_t char_index) : TokenBase(char_index, /*num_chars=*/2, RIGHT_SHIFT_TOKEN) {}
};


const std::string tokenName(unsigned int t);
Reference<TokenBase> makeTokenObject(unsigned int token_type, size_t char_index);


} // end namespace Winter
