/*=====================================================================
TokenBase.cpp
-------------
Copyright Glare Technologies Limited 2016 -
File created by ClassTemplate on Wed Oct 22 14:51:44 2008
=====================================================================*/
#include "TokenBase.h"


namespace Winter
{


const std::string tokenName(unsigned int t)
{
	switch(t) {
		case 10: return "COMMA";
		case 11: return "OPEN_PARENTHESIS";
		case 12: return "CLOSE_PARENTHESIS";
		case 13: return "OPEN_BRACE";
		case 14: return "CLOSE_BRACE";
		case 15: return "OPEN_SQUARE_BRACKET";
		case 16: return "CLOSE_SQUARE_BRACKET";
		case 17: return "COLON";
		case 18: return "RIGHT_ARROW";
		case 19: return "EQUALS";
		case 20: return "PLUS";
		case 21: return "MINUS";
		case 22: return "FORWARDS_SLASH";
		case 23: return "BACK_SLASH";
		case 24: return "ASTERISK";
		case 25: return "LEFT_ANGLE_BRACKET";
		case 26: return "RIGHT_ANGLE_BRACKET";
		case 27: return "LESS_EQUAL";
		case 28: return "GREATER_EQUAL";
		case 29: return "DOUBLE_EQUALS";
		case 30: return "NOT_EQUALS";
		case 31: return "AND";
		case 32: return "OR";
		case 33: return "EXCLAMATION_MARK";
		case 34: return "DOT";
		case 35: return "QUESTION_MARK_TOKEN";
		case BITWISE_AND_TOKEN: return "BITWISE_AND_TOKEN";
		case BITWISE_OR_TOKEN: return "BITWISE_OR_TOKEN";
		case BITWISE_XOR_TOKEN: return "BITWISE_XOR_TOKEN";
		case LEFT_SHIFT_TOKEN: return "LEFT_SHIFT_TOKEN";
		case RIGHT_SHIFT_TOKEN: return "RIGHT_SHIFT_TOKEN";
		default: return "[Unknown]";
	}
}


Reference<TokenBase> makeTokenObject(unsigned int token_type, size_t char_index)
{
	switch(token_type) {
		case 10: return Reference<TokenBase>(new COMMA_Token(char_index));
		case 11: return Reference<TokenBase>(new OPEN_PARENTHESIS_Token(char_index));
		case 12: return Reference<TokenBase>(new CLOSE_PARENTHESIS_Token(char_index));
		case 13: return Reference<TokenBase>(new OPEN_BRACE_Token(char_index));
		case 14: return Reference<TokenBase>(new CLOSE_BRACE_Token(char_index));
		case 15: return Reference<TokenBase>(new OPEN_SQUARE_BRACKET_Token(char_index));
		case 16: return Reference<TokenBase>(new CLOSE_SQUARE_BRACKET_Token(char_index));
		case 17: return Reference<TokenBase>(new COLON_Token(char_index));
		case 18: return Reference<TokenBase>(new RIGHT_ARROW_Token(char_index));
		case 19: return Reference<TokenBase>(new EQUALS_Token(char_index));
		case 20: return Reference<TokenBase>(new PLUS_Token(char_index));
		case 21: return Reference<TokenBase>(new MINUS_Token(char_index));
		case 22: return Reference<TokenBase>(new FORWARDS_SLASH_Token(char_index));
		case 23: return Reference<TokenBase>(new BACK_SLASH_Token(char_index));
		case 24: return Reference<TokenBase>(new ASTERISK_Token(char_index));
		case 25: return Reference<TokenBase>(new LEFT_ANGLE_BRACKET_Token(char_index));
		case 26: return Reference<TokenBase>(new RIGHT_ANGLE_BRACKET_Token(char_index));
		case 27: return Reference<TokenBase>(new LESS_EQUAL_Token(char_index));
		case 28: return Reference<TokenBase>(new GREATER_EQUAL_Token(char_index));
		case 29: return Reference<TokenBase>(new DOUBLE_EQUALS_Token(char_index));
		case 30: return Reference<TokenBase>(new NOT_EQUALS_Token(char_index));
		case 31: return Reference<TokenBase>(new AND_Token(char_index));
		case 32: return Reference<TokenBase>(new OR_Token(char_index));
		case 33: return Reference<TokenBase>(new EXCLAMATION_MARK_Token(char_index));
		case 34: return Reference<TokenBase>(new DOT_Token(char_index));
		case 35: return Reference<TokenBase>(new QUESTION_MARK_Token(char_index));
		case BITWISE_AND_TOKEN: return Reference<TokenBase>(new BITWISE_AND_Token(char_index));
		case BITWISE_OR_TOKEN: return Reference<TokenBase>(new BITWISE_OR_Token(char_index));\
		case BITWISE_XOR_TOKEN: return Reference<TokenBase>(new BITWISE_XOR_Token(char_index));
		case LEFT_SHIFT_TOKEN: return Reference<TokenBase>(new LEFT_SHIFT_Token(char_index));
		case RIGHT_SHIFT_TOKEN: return Reference<TokenBase>(new RIGHT_SHIFT_Token(char_index));
		default: return  Reference<TokenBase>();
	}
}


} //end namespace lang

