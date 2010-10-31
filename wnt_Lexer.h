/*=====================================================================
Lexer.h
-------
File created by ClassTemplate on Wed Jun 11 01:53:25 2008
Code By Nicholas Chapman.

Copyright 2009 Nicholas Chapman
=====================================================================*/
#ifndef __LEXER_H_666_
#define __LEXER_H_666_


#include <string>
#include <vector>
#include "TokenBase.h"
#include "BaseException.h"
#include "utils/reference.h"
class Parser;


namespace Winter
{


class LexerExcep : public BaseException
{
public:
	LexerExcep(const std::string& text_) : BaseException(text_) {}
	~LexerExcep(){}

private:
};


/*=====================================================================
Lexer
-----

=====================================================================*/
class Lexer
{
public:
	/*=====================================================================
	Lexer
	-----
	
	=====================================================================*/
	Lexer();

	~Lexer();

	static void process(const std::string& buffer, std::vector<Reference<TokenBase> >& tokens_out);

	static void test();

private:
	static const std::string errorPosition(const std::string& buffer, unsigned int pos);
	static void parseStringLiteral(Parser& parser, std::vector<Reference<TokenBase> >& tokens_out);
	static void parseNumericLiteral(Parser& parser, std::vector<Reference<TokenBase> >& tokens_out);
	static void parseIdentifier(Parser& parser, std::vector<Reference<TokenBase> >& tokens_out);
	static void parseComment(Parser& parser);
};



} //end namespace Lang


#endif //__LEXER_H_666_




