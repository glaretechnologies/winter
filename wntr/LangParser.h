/*=====================================================================
LangParser.h
------------
File created by ClassTemplate on Wed Jun 11 02:56:20 2008
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __LANGPARSER_H_666_
#define __LANGPARSER_H_666_


#include <string>
#include <vector>
#include <map>
#include "TokenBase.h"
#include "ASTNode.h"
#include "../indigo/trunk/utils/reference.h"
#include "Type.h"


namespace Winter
{


class LangParserExcep
{
public:
	LangParserExcep(const std::string& text_) : text(text_) {}
	~LangParserExcep(){}

	const std::string& what() const { return text; }
private:
	std::string text;
};


class ParseInfo
{
public:
	ParseInfo(){}
	const std::vector<Reference<TokenBase> >* tokens;
	const char* text_buffer;
	unsigned int* i;
};

/*=====================================================================
LangParser
----------

=====================================================================*/
class LangParser
{
public:
	LangParser();

	~LangParser();

	Reference<ASTNode> parseBuffer(const std::vector<Reference<TokenBase> >& tokens, const char* text_buffer);

	static void test();

private:
	const std::string errorPosition(const std::string& buffer, unsigned int pos);

	const std::string parseIdentifier(const std::string& id_type, const std::vector<Reference<TokenBase> >& tokens, const char* text_buffer, unsigned int& i);

	Reference<FunctionDefinition> parseFunctionDefinition(ASTNode* parent, const std::vector<Reference<TokenBase> >& tokens, const char* text_buffer, unsigned int& i);
	//Reference<ASTNode> parseFunctionDeclaration(const std::vector<Reference<TokenBase> >& tokens, const char* text_buffer, unsigned int& i);

	Reference<ASTNode> parseFunctionExpression(ASTNode* parent, const std::vector<Reference<TokenBase> >& tokens, const char* text_buffer, unsigned int& i);

	Reference<ASTNode> parseExpression(ASTNode* parent, const std::vector<Reference<TokenBase> >& tokens, const char* text_buffer, unsigned int& i);
	ASTNodeRef parseBasicExpression(ASTNode* parent, const std::vector<Reference<TokenBase> >& tokens, const char* text_buffer, unsigned int& i);

	void parseToken(const std::vector<Reference<TokenBase> >& tokens, const char* text_buffer, unsigned int token_type, const std::string& type_name, unsigned int& i);

	ASTNodeRef parseVariableExpression(ASTNode* parent, const std::vector<Reference<TokenBase> >& tokens, const char* text_buffer, unsigned int& i);

	TypeRef parseType(const std::vector<Reference<TokenBase> >& tokens, const char* text_buffer, unsigned int& i);
	TypeRef parseMapType(const std::vector<Reference<TokenBase> >& tokens, const char* text_buffer, unsigned int& i);

	ASTNodeRef parseAddSubExpression(ASTNode* parent, const std::vector<Reference<TokenBase> >& tokens, const char* text_buffer, unsigned int& i);
	ASTNodeRef parseMulDivExpression(ASTNode* parent, const std::vector<Reference<TokenBase> >& tokens, const char* text_buffer, unsigned int& i);
	ASTNodeRef parseParenExpression(ASTNode* parent, const std::vector<Reference<TokenBase> >& tokens, const char* text_buffer, unsigned int& i);
	ASTNodeRef parseMapLiteralExpression(ASTNode* parent, const std::vector<Reference<TokenBase> >& tokens, const char* text_buffer, unsigned int& i);
	Reference<LetASTNode> parseLet(ASTNode* parent, const ParseInfo& parseinfo);
};


} //end namespace Lang


#endif //__LANGPARSER_H_666_
