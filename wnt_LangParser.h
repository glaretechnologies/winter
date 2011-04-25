/*=====================================================================
LangParser.h
------------
File created by ClassTemplate on Wed Jun 11 02:56:20 2008
Code By Nicholas Chapman.

Copyright 2009 Nicholas Chapman
=====================================================================*/
#ifndef __LANGPARSER_H_666_
#define __LANGPARSER_H_666_


#include <string>
#include <vector>
#include <map>
#include "TokenBase.h"
#include "BaseException.h"
#include "wnt_ASTNode.h"
#include "wnt_FunctionDefinition.h"
#include "utils/reference.h"
#include "wnt_Type.h"


namespace Winter
{


class LangParserExcep : public BaseException
{
public:
	LangParserExcep(const std::string& text_) : BaseException(text_) {}
	~LangParserExcep(){}
private:
};


class ParseInfo
{
public:
	ParseInfo(unsigned int& i_, const std::vector<Reference<TokenBase> >& t, std::map<std::string, TypeRef>& named_types_,
		vector<FunctionDefinitionRef>& func_defs_) 
		: i(i_), tokens(t), named_types(named_types_), func_defs(func_defs_) {}
	const std::vector<Reference<TokenBase> >& tokens;
	const char* text_buffer;
	unsigned int& i;
	std::map<std::string, TypeRef>& named_types;
	vector<FunctionDefinitionRef>& func_defs;
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

	Reference<ASTNode> parseBuffer(const std::vector<Reference<TokenBase> >& tokens, const char* text_buffer,
		vector<FunctionDefinitionRef>& func_defs_out);

	static void test();

private:
	const std::string errorPosition(const std::string& buffer, unsigned int pos);
	const std::string errorPosition(const ParseInfo& parseinfo);
	const std::string errorPositionPrevToken(const ParseInfo& parseinfo);

	const std::string parseIdentifier(const std::string& id_type, const ParseInfo& parseinfo);
	ASTNodeRef parseLiteral(const ParseInfo& parseinfo);
	Reference<IntLiteral> parseIntLiteral(const ParseInfo& parseinfo);

	FunctionDefinitionRef parseFunctionDefinition(const ParseInfo& parseinfo);
	FunctionDefinitionRef parseFunctionDefinitionGivenName(const std::string& func_name, const ParseInfo& parseinfo);
	//Reference<ASTNode> parseFunctionDeclaration(const std::vector<Reference<TokenBase> >& tokens, const char* text_buffer, unsigned int& i);

	Reference<ASTNode> parseFunctionExpression(const ParseInfo& parseinfo);

	Reference<ASTNode> parseLetBlock(const ParseInfo& parseinfo);
	Reference<ASTNode> parseExpression(const ParseInfo& parseinfo);
	ASTNodeRef parseBasicExpression(const ParseInfo& parseinfo);

	//void parseToken(const std::vector<Reference<TokenBase> >& tokens, const char* text_buffer, unsigned int token_type, const std::string& type_name, unsigned int& i);
	void parseToken(unsigned int token_type, const ParseInfo& parseinfo);
	bool isTokenCurrent(unsigned int token_type, const ParseInfo& parseinfo);

	ASTNodeRef parseFieldExpression(const ParseInfo& parseinfo);
	ASTNodeRef parseVariableExpression(const ParseInfo& parseinfo);

	TypeRef parseType(const ParseInfo& parseinfo, const std::vector<std::string>& generic_type_params);
	TypeRef parseMapType(const ParseInfo& parseinfo, const std::vector<std::string>& generic_type_params);
	TypeRef parseArrayType(const ParseInfo& parseinfo, const std::vector<std::string>& generic_type_params);
	TypeRef parseFunctionType(const ParseInfo& p, const std::vector<std::string>& generic_type_params);
	Reference<StructureType> parseStructType(const ParseInfo& p, const std::vector<std::string>& generic_type_params);
	TypeRef parseVectorType(const ParseInfo& p, const std::vector<std::string>& generic_type_params);

	ASTNodeRef parseComparisonExpression(const ParseInfo& parseinfo);
	ASTNodeRef parseUnaryExpression(const ParseInfo& parseinfo);
	ASTNodeRef parseAddSubExpression(const ParseInfo& parseinfo);
	ASTNodeRef parseMulDivExpression(const ParseInfo& parseinfo);
	ASTNodeRef parseParenExpression(const ParseInfo& parseinfo);
	ASTNodeRef parseMapLiteralExpression(const ParseInfo& parseinfo);
	ASTNodeRef parseArrayOrVectorLiteralExpression(const ParseInfo& parseinfo);
	Reference<LetASTNode> parseLet(const ParseInfo& parseinfo);
	FunctionDefinitionRef parseAnonFunction(const ParseInfo& parseinfo);
	void parseParameterList(const ParseInfo& parseinfo, const std::vector<std::string>& generic_type_params, std::vector<FunctionDefinition::FunctionArg>& args_out);

	bool isKeyword(const std::string& name);
private:
	std::vector<unsigned int> comparison_tokens;

};


} //end namespace Lang


#endif //__LANGPARSER_H_666_
