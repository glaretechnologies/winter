/*=====================================================================
Lexer.h
-------
Copyright Glare Technologies Limited 2016 -
File created by ClassTemplate on Wed Jun 11 01:53:25 2008
=====================================================================*/
#pragma once


#include "TokenBase.h"
#include "BaseException.h"
#include "wnt_SourceBuffer.h"
#include <utils/Reference.h>
#include <string>
#include <vector>
class Parser;


namespace Winter
{


class LexerExcep : public ExceptionWithPosition
{
public:
	LexerExcep(const std::string& text_, const BufferPosition& pos) : ExceptionWithPosition(text_, pos) {}
};


/*=====================================================================
Lexer
-----

=====================================================================*/
class Lexer
{
public:
	static void process(const SourceBufferRef& src, std::vector<Reference<TokenBase> >& tokens_out);

	static void test();

private:
	static BufferPosition errorPosition(const SourceBufferRef& buffer, size_t pos);
	static void parseStringLiteral(const SourceBufferRef& buffer, Parser& parser, std::vector<Reference<TokenBase> >& tokens_out);
	static void parseCharLiteral(const SourceBufferRef& buffer, Parser& parser, std::vector<Reference<TokenBase> >& tokens_out);
	static void parseNumericLiteral(const SourceBufferRef& buffer, Parser& parser, std::vector<Reference<TokenBase> >& tokens_out);
	static void parseIdentifier(const SourceBufferRef& buffer, Parser& parser, std::vector<Reference<TokenBase> >& tokens_out);
	static void parseComment(const SourceBufferRef& buffer, Parser& parser);
	static void parseUnicodeEscapedChar(const SourceBufferRef& buffer, Parser& parser, std::string& s);
};


} //end namespace Winter
