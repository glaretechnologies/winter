/*=====================================================================
Lexer.cpp
---------
File created by ClassTemplate on Wed Jun 11 01:53:25 2008
Code By Nicholas Chapman.

Copyright 2009 Nicholas Chapman
=====================================================================*/
#include "wnt_Lexer.h"


#include "utils/Parser.h"
#include "indigo/TestUtils.h"
#include "maths/mathstypes.h"


namespace Winter
{


Lexer::Lexer()
{
}


Lexer::~Lexer()
{
}


void Lexer::parseStringLiteral(Parser& parser, std::vector<Reference<TokenBase> >& tokens_out)
{
	assert(parser.notEOF());
	assert(parser.current() == '"');
	
	const unsigned int char_index = parser.currentPos();

	parser.advance();

	std::string s = "";
	while(1)
	{
		if(parser.eof())
			throw LexerExcep("End of input while parsing string literal." + errorPosition(parser.getText(), parser.currentPos()));

		if(parser.current() == '"')
		{
			parser.advance();
			break;
		}

		s = ::appendChar(s, parser.current());
		parser.advance();
	}

	tokens_out.push_back(Reference<TokenBase>(new StringLiteralToken(s, char_index)));
}


static void parseWhiteSpace(Parser& parser, std::vector<Reference<TokenBase> >& tokens_out)
{
	parser.parseWhiteSpace();
}


void Lexer::parseNumericLiteral(Parser& parser, std::vector<Reference<TokenBase> >& tokens_out)
{
	assert(parser.notEOF());
	
	const unsigned int char_index = parser.currentPos();

	if(parser.fractionalNumberNext())
	{
		double x;
		if(!parser.parseDouble(x))
			throw LexerExcep("Failed to parse real." + errorPosition(parser.getText(), parser.currentPos()));

		tokens_out.push_back(Reference<TokenBase>(new FloatLiteralToken((float)x, char_index)));
	}
	else
	{
		int x;
		if(!parser.parseInt(x))
		{
			const unsigned int pos = parser.currentPos();
			std::string next_token;
			parser.parseNonWSToken(next_token);
			throw LexerExcep("Failed to parse int.  (Next chars '" + next_token + "')" + errorPosition(parser.getText(), pos));
		}

		tokens_out.push_back(Reference<TokenBase>(new IntLiteralToken(x, char_index)));
	}
}


void Lexer::parseIdentifier(Parser& parser, std::vector<Reference<TokenBase> >& tokens_out)
{
	assert(parser.notEOF());
	assert(::isAlphabetic(parser.current()));

	const unsigned int char_index = parser.currentPos();

	std::string s;
	while(parser.notEOF() && (::isAlphaNumeric(parser.current()) || parser.current() == '_'))
	{
		s = ::appendChar(s, parser.current());
		parser.advance();
	}

	if(s == "true")
		tokens_out.push_back(Reference<TokenBase>(new BoolLiteralToken(true, char_index)));
	else if(s == "false")
		tokens_out.push_back(Reference<TokenBase>(new BoolLiteralToken(false, char_index)));
	else
		tokens_out.push_back(Reference<TokenBase>(new IdentifierToken(s, char_index)));
}


void Lexer::parseComment(Parser& parser)
{
	assert(parser.notEOF());
	assert(parser.current() == '#');

	parser.advancePastLine();
}


void Lexer::process(const std::string& buffer, std::vector<Reference<TokenBase> >& tokens_out)
{
	Parser parser(buffer.c_str(), (unsigned int)buffer.length());

	while(parser.notEOF())
	{
		if(parser.current() == '"')
		{
			parseStringLiteral(parser, tokens_out);
		}
		else if(parser.current() == '-' /*|| parser.current() == '+'*/ /*|| parser.current() == '.'*/ || ::isNumeric(parser.current()))
		{
			if(parser.parseString("->"))
			{
				tokens_out.push_back(Reference<TokenBase>(new RIGHT_ARROW_Token(parser.currentPos() - 2)));
			}
			else
			{
				if(parser.current() == '-')
				{
					if(::isNumeric(parser.next()))
					{
						// This is a negative numeric literal like '-3.0f'
						parseNumericLiteral(parser, tokens_out);
					}
					else
					{
						tokens_out.push_back(Reference<TokenBase>(new MINUS_Token(parser.currentPos())));
						parser.advance();
					}
				}
				else
				{
					parseNumericLiteral(parser, tokens_out);
				}

				/*if((parser.current() == '-' || parser.current() == '+') && 
					(parser.current() == '+' || 
						(parser.currentPos() + 1 < parser.getTextSize() && isWhitespace(parser.getText()[parser.currentPos() + 1]))
					)
				)
				{
					// this is the binary infix operator - or +
					
					if(parser.current() == '+')
					{
						tokens_out.push_back(Reference<TokenBase>(new PLUS_Token(parser.currentPos())));
						parser.advance();
					}
					else if(parser.current() == '-')
					{
						tokens_out.push_back(Reference<TokenBase>(new MINUS_Token(parser.currentPos())));
						parser.advance();
					}
				}
				else
				{
					parseNumericLiteral(parser, tokens_out);
				}
				*/
			}
		}
		else if(::isWhitespace(parser.current()))
		{
			parseWhiteSpace(parser, tokens_out);
		}
		else if(::isAlphabetic(parser.current()))
		{
			parseIdentifier(parser, tokens_out);
		}
		else if(parser.current() == ',')
		{
			tokens_out.push_back(Reference<TokenBase>(new COMMA_Token(parser.currentPos())));
			parser.advance();
		}
		else if(parser.current() == '(')
		{
			tokens_out.push_back(Reference<TokenBase>(new OPEN_PARENTHESIS_Token(parser.currentPos())));
			parser.advance();
		}
		else if(parser.current() == ')')
		{
			tokens_out.push_back(Reference<TokenBase>(new CLOSE_PARENTHESIS_Token(parser.currentPos())));
			parser.advance();
		}
		else if(parser.current() == '[')
		{
			tokens_out.push_back(Reference<TokenBase>(new OPEN_SQUARE_BRACKET_Token(parser.currentPos())));
			parser.advance();
		}
		else if(parser.current() == ']')
		{
			tokens_out.push_back(Reference<TokenBase>(new CLOSE_SQUARE_BRACKET_Token(parser.currentPos())));
			parser.advance();
		}
		else if(parser.current() == '{')
		{
			tokens_out.push_back(Reference<TokenBase>(new OPEN_BRACE_Token(parser.currentPos())));
			parser.advance();
		}
		else if(parser.current() == '}')
		{
			tokens_out.push_back(Reference<TokenBase>(new CLOSE_BRACE_Token(parser.currentPos())));
			parser.advance();
		}
		else if(parser.current() == ':')
		{
			tokens_out.push_back(Reference<TokenBase>(new COLON_Token(parser.currentPos())));
			parser.advance();
		}
		else if(parser.current() == '#')
		{
			parseComment(parser);
		}
		else if(parser.current() == '+')
		{
			tokens_out.push_back(Reference<TokenBase>(new PLUS_Token(parser.currentPos())));
			parser.advance();
		}
		else if(parser.current() == '-')
		{
			tokens_out.push_back(Reference<TokenBase>(new MINUS_Token(parser.currentPos())));
			parser.advance();
		}
		else if(parser.current() == '/')
		{
			tokens_out.push_back(Reference<TokenBase>(new FORWARDS_SLASH_Token(parser.currentPos())));
			parser.advance();
		}
		else if(parser.current() == '*')
		{
			tokens_out.push_back(Reference<TokenBase>(new ASTERISK_Token(parser.currentPos())));
			parser.advance();
		}
		else if(parser.current() == '\\')
		{
			tokens_out.push_back(Reference<TokenBase>(new BACK_SLASH_Token(parser.currentPos())));
			parser.advance();
		}
		else if(parser.current() == '=')
		{
			parser.advance();

			if(parser.notEOF() && parser.current() == '=')
			{
				tokens_out.push_back(Reference<TokenBase>(new DOUBLE_EQUALS_Token(parser.currentPos())));
				parser.advance();
			}
			else
				tokens_out.push_back(Reference<TokenBase>(new EQUALS_Token(parser.currentPos())));
				
		}
		else if(parser.current() == '!')
		{
			tokens_out.push_back(Reference<TokenBase>(new NOT_EQUALS_Token(parser.currentPos())));
			if(!parser.parseString("!="))
				throw LexerExcep("Error while parsing '!='" + errorPosition(buffer, parser.currentPos()));
		}
		else if(parser.current() == '.')
		{
			tokens_out.push_back(Reference<TokenBase>(new DOT_Token(parser.currentPos())));
			parser.advance();
		}
		else if(parser.current() == '|')
		{
			tokens_out.push_back(Reference<TokenBase>(new OR_Token(parser.currentPos())));
			if(!parser.parseString("||"))
				throw LexerExcep("Error while parsing '||'" + errorPosition(buffer, parser.currentPos()));
		}
		else if(parser.current() == '&')
		{
			tokens_out.push_back(Reference<TokenBase>(new AND_Token(parser.currentPos())));
			if(!parser.parseString("&&"))
				throw LexerExcep("Error while parsing '&&'" + errorPosition(buffer, parser.currentPos()));
		}
		else if(parser.current() == '<')
		{
			if(parser.parseString("<="))
				tokens_out.push_back(Reference<TokenBase>(new LESS_EQUAL_Token(parser.currentPos())));
			else
			{
				tokens_out.push_back(Reference<TokenBase>(new LEFT_ANGLE_BRACKET_Token(parser.currentPos())));
				parser.advance();
			}
		}
		else if(parser.current() == '>')
		{
			if(parser.parseString(">="))
				tokens_out.push_back(Reference<TokenBase>(new GREATER_EQUAL_Token(parser.currentPos())));
			else
			{
				tokens_out.push_back(Reference<TokenBase>(new RIGHT_ANGLE_BRACKET_Token(parser.currentPos())));
				parser.advance();
			}
		}
		else
		{
			throw LexerExcep("Invalid character '" + std::string(1, parser.current()) + "'." + errorPosition(buffer, parser.currentPos()));
		}
	}
}


const std::string Lexer::errorPosition(const std::string& buffer, unsigned int pos)
{
	unsigned int line, col;
	StringUtils::getPosition(buffer, pos, line, col);
	return "  Line " + toString(line + 1) + ", column " + toString(col + 1);
}


void Lexer::test()
{
	const std::string s = "-34.546e2 \"hello\" whats_up123 \t \"meh\"123:(false";
	std::vector<Reference<TokenBase> > t;
	process(s, t);

	testAssert(t.size() == 8);

	testAssert(t[0]->getType() == FLOAT_LITERAL_TOKEN);
	testAssert(::epsEqual(t[0]->getFloatLiteralValue(), -34.546e2f));

	testAssert(t[1]->getType() == STRING_LITERAL_TOKEN);
	testAssert(t[1]->getStringLiteralValue() == "hello");

	testAssert(t[2]->getType() == IDENTIFIER_TOKEN);
	testAssert(t[2]->getIdentifierValue() == "whats_up123");

	testAssert(t[3]->getType() == STRING_LITERAL_TOKEN);
	testAssert(t[3]->getStringLiteralValue() == "meh");

	testAssert(t[4]->getType() == INT_LITERAL_TOKEN);
	testAssert(t[4]->getIntLiteralValue() == 123);

	testAssert(t[5]->getType() == COLON_TOKEN);

	testAssert(t[6]->getType() == OPEN_PARENTHESIS_TOKEN);

	testAssert(t[7]->getType() == BOOL_LITERAL_TOKEN);
	testAssert(t[7]->getBoolLiteralValue() == false);

}


} //end namespace Lang
