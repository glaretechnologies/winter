/*=====================================================================
LangParser.h
------------
Copyright Glare Technologies Limited 2014 -
File created by ClassTemplate on Wed Jun 11 02:56:20 2008
=====================================================================*/
#pragma once


#include "TokenBase.h"
#include "BaseException.h"
#include "wnt_ASTNode.h"
#include "wnt_FunctionDefinition.h"
#include "wnt_Type.h"
#include "wnt_SourceBuffer.h"
#include <utils/Reference.h>
#include <string>
#include <vector>
#include <map>


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
		std::vector<FunctionDefinitionRef>& func_defs_) 
		: i(i_), tokens(t), named_types(named_types_), func_defs(func_defs_), else_token_present(false) {}
	const std::vector<Reference<TokenBase> >& tokens;
	//const char* text_buffer;
	//const std::string* text_buffer;
	const SourceBuffer* text_buffer;
	unsigned int& i;
	std::map<std::string, TypeRef>& named_types;
	std::vector<FunctionDefinitionRef>& func_defs;
	bool else_token_present;
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

	Reference<ASTNode> parseBuffer(const std::vector<Reference<TokenBase> >& tokens, 
		const SourceBufferRef& source_buffer,
		std::vector<FunctionDefinitionRef>& func_defs_out, 
		std::map<std::string, TypeRef>& named_types);

	static void test();

private:
	const std::string errorPosition(const SourceBuffer& buffer, unsigned int pos);
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
	ASTNodeRef parseIfExpression(const ParseInfo& parseinfo);

	TypeRef parseType(const ParseInfo& parseinfo, const std::vector<std::string>& generic_type_params);
	TypeRef parseSumType(const ParseInfo& parseinfo, const std::vector<std::string>& generic_type_params);
	TypeRef parseElementaryType(const ParseInfo& parseinfo, const std::vector<std::string>& generic_type_params);
	TypeRef parseMapType(const ParseInfo& parseinfo, const std::vector<std::string>& generic_type_params);
	TypeRef parseArrayType(const ParseInfo& parseinfo, const std::vector<std::string>& generic_type_params);
	TypeRef parseFunctionType(const ParseInfo& p, const std::vector<std::string>& generic_type_params);
	Reference<StructureType> parseStructType(const ParseInfo& p, const std::vector<std::string>& generic_type_params);
	TypeRef parseVectorType(const ParseInfo& p, const std::vector<std::string>& generic_type_params);

	ASTNodeRef parseComparisonExpression(const ParseInfo& parseinfo);
	ASTNodeRef parseUnaryExpression(const ParseInfo& parseinfo);
	ASTNodeRef parseAddSubExpression(const ParseInfo& parseinfo);
	ASTNodeRef parseMulDivExpression(const ParseInfo& parseinfo);
	ASTNodeRef parseBinaryLogicalExpression(const ParseInfo& parseinfo);
	ASTNodeRef parseParenExpression(const ParseInfo& parseinfo);
	ASTNodeRef parseMapLiteralExpression(const ParseInfo& parseinfo);
	ASTNodeRef parseArrayOrVectorLiteralOrArraySubscriptExpression(const ParseInfo& parseinfo);
	Reference<LetASTNode> parseLet(const ParseInfo& parseinfo);
	FunctionDefinitionRef parseAnonFunction(const ParseInfo& parseinfo);
	void parseParameterList(const ParseInfo& parseinfo, const std::vector<std::string>& generic_type_params, std::vector<FunctionDefinition::FunctionArg>& args_out);

	bool isKeyword(const std::string& name);
private:
	std::vector<unsigned int> comparison_tokens;

};


}
