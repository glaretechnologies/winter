/*=====================================================================
Lexer.cpp
---------
File created by ClassTemplate on Wed Jun 11 01:53:25 2008
Code By Nicholas Chapman.

Copyright 2009 Nicholas Chapman
=====================================================================*/
#include "wnt_Lexer.h"


#include "wnt_Diagnostics.h"
#include "utils/Parser.h"
#include "utils/UTF8Utils.h"
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


// Parse a single escaped character, append UTF-8 representation to s.
void Lexer::parseUnicodeEscapedChar(const SourceBufferRef& buffer, Parser& parser, std::string& s)
{
	assert(parser.current() == 'u');

	parser.advance(); // Consume 'u'

	if(parser.eof() || parser.current() != '{')
		throw LexerExcep("Error while parsing character literal, was expecting a '{'." + errorPosition(buffer, parser.currentPos()));

	// Parse Unicode code point literal (like \u{7FFF})
	parser.advance(); // Consume '{'

	std::string code_point_s;
	while(1)
	{
		if(parser.eof())
			throw LexerExcep("End of input while parsing char literal." + errorPosition(buffer, parser.currentPos()));
				
		if(parser.current() == '}')
		{
			parser.advance(); // Consume '}'
			break;
		}

		if(::isNumeric(parser.current()) || (parser.current() >= 'a' && parser.current() <= 'f') || (parser.current() >= 'A' && parser.current() <= 'F'))
		{
			code_point_s.push_back(parser.current());
			parser.advance();
		}
		else
			throw LexerExcep("Error while parsing escape sequence in character literal: invalid character '" + std::string(1, parser.current()) + "'." + errorPosition(buffer, parser.currentPos()));
	}

	try
	{
		const uint32 code_point = hexStringToUInt32(code_point_s);
		if(code_point > 0x10FFFF)
			throw LexerExcep("Invalid code point '" + code_point_s + "'." + errorPosition(buffer, parser.currentPos()));

		// Convert code point to UTF-8
		s += UTF8Utils::encodeCodePoint(code_point);
	}
	catch(StringUtilsExcep& e)
	{
		throw LexerExcep("Error while parsing escape sequence: " + e.what() + " " + errorPosition(buffer, parser.currentPos()));
	}
}


void Lexer::parseStringLiteral(const SourceBufferRef& buffer, Parser& parser, std::vector<Reference<TokenBase> >& tokens_out)
{
	assert(parser.notEOF());
	assert(parser.current() == '"');
	
	const unsigned int char_index = parser.currentPos();

	parser.advance();

	std::string s;
	while(1)
	{
		if(parser.eof())
			throw LexerExcep("End of input while parsing string literal." + errorPosition(buffer, parser.currentPos() - 1));

		if(parser.current() == '\\') // If char is backslash, parse escape sequence:
		{
			parser.advance(); // Consume '\'
			if(parser.eof())
				throw LexerExcep("End of input while parsing string literal." + errorPosition(buffer, parser.currentPos()));

			if(parser.current() == '"')
			{
				s.push_back('"');
				parser.advance();
			}
			else if(parser.current() == 'n')
			{
				s.push_back('\n');
				parser.advance();
			}
			else if(parser.current() == 'r')
			{
				s.push_back('\r');
				parser.advance();
			}
			else if(parser.current() == 't')
			{
				s.push_back('\t');
				parser.advance();
			}
			else if(parser.current() == '\\')
			{
				s.push_back('\\');
				parser.advance();
			}
			else if(parser.current() == 'u')
			{
				parseUnicodeEscapedChar(buffer, parser, s);
			}
			else
				throw LexerExcep("Invalid escape character." + errorPosition(buffer, parser.currentPos()));
		}
		else if(parser.current() == '"') // If got to end of string literal:
		{
			parser.advance(); // Consume '"'
			break;
		}
		else
		{
			s.push_back(parser.current());
			parser.advance();
		}
	}

	tokens_out.push_back(new StringLiteralToken(s, char_index));
}


void Lexer::parseCharLiteral(const SourceBufferRef& buffer, Parser& parser, std::vector<Reference<TokenBase> >& tokens_out)
{
	assert(parser.notEOF());
	assert(parser.current() == '\'');
	
	const unsigned int char_index = parser.currentPos();

	parser.advance(); // Consume '

	std::string s;
	if(parser.eof())
		throw LexerExcep("End of input while parsing char literal." + errorPosition(buffer, parser.currentPos()));

	if(parser.current() == '\\') // If char is backslash, parse escape sequence:
	{
		parser.advance(); // Consume '\'
		if(parser.eof())
			throw LexerExcep("End of input while parsing char literal." + errorPosition(buffer, parser.currentPos()));

		if(parser.current() == '\'')
		{
			s.push_back('\'');
			parser.advance();
		}
		else if(parser.current() == 'n')
		{
			s.push_back('\n');
			parser.advance();
		}
		else if(parser.current() == 'r')
		{
			s.push_back('\r');
			parser.advance();
		}
		else if(parser.current() == 't')
		{
			s.push_back('\t');
			parser.advance();
		}
		else if(parser.current() == '\\')
		{
			s.push_back('\\');
			parser.advance();
		}
		else if(parser.current() == 'u')
		{
			parseUnicodeEscapedChar(buffer, parser, s);
		}
		else
			throw LexerExcep("Invalid escape character." + errorPosition(buffer, parser.currentPos()));
	}
	else
	{
		s += parser.current();
		parser.advance();
	}

	// Parse trailing single-quote
	if(parser.eof() || (parser.current() != '\''))
		throw LexerExcep("Error while parsing character literal, was expecting a single quote." + errorPosition(buffer, parser.currentPos()));
	parser.advance(); // Consume '

	tokens_out.push_back(new CharLiteralToken(s, char_index));
}


static void parseWhiteSpace(const SourceBufferRef& buffer, Parser& parser, std::vector<Reference<TokenBase> >& tokens_out)
{
	parser.parseWhiteSpace();
}


void Lexer::parseNumericLiteral(const SourceBufferRef& buffer, Parser& parser, std::vector<Reference<TokenBase> >& tokens_out)
{
	assert(parser.notEOF());
	
	const unsigned int char_index = parser.currentPos();

	if(parser.fractionalNumberNext())
	{
		double x;
		if(!parser.parseDouble(x))
			throw LexerExcep("Failed to parse real." + errorPosition(buffer, parser.currentPos()));

		const char lastchar = parser.prev();
		char suffix = 0;
		if(lastchar == 'f' || lastchar == 'd')
			suffix = lastchar;
		tokens_out.push_back(Reference<TokenBase>(new FloatLiteralToken(x, suffix, char_index)));
	}
	else
	{
		int64 x;
		if(!parser.parseInt64(x))
		{
			const unsigned int pos = parser.currentPos();
			string_view next_token;
			parser.parseNonWSToken(next_token);
			throw LexerExcep("Failed to parse int.  (Next chars '" + next_token.to_string() + "')" + errorPosition(buffer, pos));
		}

		int num_bits = 32;
		// Parse suffix if present

		const unsigned int suffix_pos = parser.currentPos(); // Suffix position (if present)
		bool is_signed = true;
		if(parser.currentIsChar('i'))
		{
			parser.advance();
			if(!parser.parseInt(num_bits))
			{
				const unsigned int pos = parser.currentPos();
				string_view next_token;
				parser.parseNonWSToken(next_token);
				throw LexerExcep("Failed to parse integer suffix after 'i':.  (Next chars '" + next_token.to_string() + "')" + errorPosition(buffer, pos));
			}
		}
		else if(parser.currentIsChar('u'))
		{
			is_signed = false;
			parser.advance();
			// If there is a number after the 'u', parse it, otherwise use default bitness (32).
			if(parser.notEOF() && isNumeric(parser.current()))
			{
				if(!parser.parseInt(num_bits))
				{
					const unsigned int pos = parser.currentPos();
					string_view next_token;
					parser.parseNonWSToken(next_token);
					throw LexerExcep("Failed to parse integer suffix after 'u':.  (Next chars '" + next_token.to_string() + "')" + errorPosition(buffer, pos));
				}
			}
		}

		if(!(num_bits == 16 || num_bits == 32 || num_bits == 64))
			throw LexerExcep("Integer must have 16, 32 or 64 bits." + errorPosition(buffer, suffix_pos));

		tokens_out.push_back(Reference<TokenBase>(new IntLiteralToken(x, num_bits, is_signed, char_index)));
	}
}


void Lexer::parseIdentifier(const SourceBufferRef& buffer, Parser& parser, std::vector<Reference<TokenBase> >& tokens_out)
{
	assert(parser.notEOF());
	assert(::isAlphabetic(parser.current()) || parser.current() == '_');

	const unsigned int char_index = parser.currentPos();

	std::string s;
	while(parser.notEOF() && (::isAlphaNumeric(parser.current()) || parser.current() == '_'))
	{
		s.push_back(parser.current());
		parser.advance();
	}

	if(s == "true")
		tokens_out.push_back(Reference<TokenBase>(new BoolLiteralToken(true, char_index)));
	else if(s == "false")
		tokens_out.push_back(Reference<TokenBase>(new BoolLiteralToken(false, char_index)));
	else
		tokens_out.push_back(Reference<TokenBase>(new IdentifierToken(s, char_index)));
}


void Lexer::parseComment(const SourceBufferRef& buffer, Parser& parser)
{
	assert(parser.notEOF());
	assert(parser.current() == '#');

	parser.advancePastLine();
}


void Lexer::process(const SourceBufferRef& src, std::vector<Reference<TokenBase> >& tokens_out)
{
	Parser parser(src->source.c_str(), (unsigned int)src->source.length());

	while(parser.notEOF())
	{
		if(parser.current() == '"')
		{
			parseStringLiteral(src, parser, tokens_out);
		}
		else if(parser.current() == '\'')
		{
			parseCharLiteral(src, parser, tokens_out);
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
					if(parser.nextIsNotEOF() && ::isNumeric(parser.next()))
					{
						// This is a negative numeric literal like '-3.0f'
						parseNumericLiteral(src, parser, tokens_out);
					}
					else
					{
						tokens_out.push_back(Reference<TokenBase>(new MINUS_Token(parser.currentPos())));
						parser.advance();
					}
				}
				else
				{
					parseNumericLiteral(src, parser, tokens_out);
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
			parseWhiteSpace(src, parser, tokens_out);
		}
		else if(::isAlphabetic(parser.current()) || parser.current() == '_')
		{
			parseIdentifier(src, parser, tokens_out);
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
			parseComment(src, parser);
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
			parser.advance();
			if(parser.currentIsChar('='))
			{
				tokens_out.push_back(Reference<TokenBase>(new NOT_EQUALS_Token(parser.currentPos())));
				parser.advance();
			}
			else
			{
				tokens_out.push_back(Reference<TokenBase>(new EXCLAMATION_MARK_Token(parser.currentPos())));
			}
		}
		else if(parser.current() == '.')
		{
			tokens_out.push_back(Reference<TokenBase>(new DOT_Token(parser.currentPos())));
			parser.advance();
		}
		else if(parser.current() == '|')
		{
			parser.advance();
			if(parser.currentIsChar('|'))
			{
				tokens_out.push_back(Reference<TokenBase>(new OR_Token(parser.currentPos() - 1)));
				parser.advance();
			}
			else
				tokens_out.push_back(Reference<TokenBase>(new BITWISE_OR_Token(parser.currentPos() - 1)));
		}
		else if(parser.current() == '&')
		{
			parser.advance();
			if(parser.currentIsChar('&'))
			{
				tokens_out.push_back(Reference<TokenBase>(new AND_Token(parser.currentPos())));
				parser.advance();
			}
			else
				tokens_out.push_back(Reference<TokenBase>(new BITWISE_AND_Token(parser.currentPos())));
		}
		else if(parser.current() == '<')
		{
			parser.advance(); // Consume the '<'.
			if(parser.currentIsChar('='))
			{
				parser.advance();
				tokens_out.push_back(Reference<TokenBase>(new LESS_EQUAL_Token(parser.currentPos())));
			}
			else if(parser.currentIsChar('<'))
			{
				parser.advance();
				tokens_out.push_back(Reference<TokenBase>(new LEFT_SHIFT_Token(parser.currentPos())));
			}
			else
				tokens_out.push_back(Reference<TokenBase>(new LEFT_ANGLE_BRACKET_Token(parser.currentPos())));
		}
		else if(parser.current() == '>')
		{
			parser.advance(); // Consume the '>'.
			if(parser.currentIsChar('='))
			{
				parser.advance();
				tokens_out.push_back(Reference<TokenBase>(new GREATER_EQUAL_Token(parser.currentPos())));
			}
			else if(parser.currentIsChar('>'))
			{
				parser.advance();
				tokens_out.push_back(Reference<TokenBase>(new RIGHT_SHIFT_Token(parser.currentPos())));
			}
			else
				tokens_out.push_back(Reference<TokenBase>(new RIGHT_ANGLE_BRACKET_Token(parser.currentPos())));
		}
		else if(parser.current() == '^')
		{
			parser.advance();
			tokens_out.push_back(Reference<TokenBase>(new BITWISE_XOR_Token(parser.currentPos())));
		}
		else if(parser.current() == '?')
		{
			tokens_out.push_back(Reference<TokenBase>(new QUESTION_MARK_Token(parser.currentPos())));
			parser.advance();
		}
		else
		{
			throw LexerExcep("Invalid character '" + std::string(1, parser.current()) + "'." + errorPosition(src, parser.currentPos()));
		}
	}
}


const std::string Lexer::errorPosition(const SourceBufferRef& buffer, unsigned int pos)
{
	return Diagnostics::positionString(*buffer, myMin(pos, (unsigned int)buffer->source.size() - 1));
}


#if BUILD_TESTS


void Lexer::test()
{
	const std::string s = "-34.546e2 \"hello\" whats_up123 \t \"meh\"123:(false";
	SourceBufferRef buffer(new SourceBuffer("buffer", s));

	std::vector<Reference<TokenBase> > t;
	process(buffer, t);

	testAssert(t.size() == 8);

	testAssert(t[0]->getType() == FLOAT_LITERAL_TOKEN);
	testAssert(::epsEqual(t[0]->getFloatLiteralValue(), -34.546e2));

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


#endif


} //end namespace Lang
