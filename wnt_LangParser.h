/*=====================================================================
LangParser.h
------------
Copyright Glare Technologies Limited 2016 -
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
};


class ParseInfo
{
public:
	ParseInfo(unsigned int& i_, const std::vector<Reference<TokenBase> >& t, std::map<std::string, TypeVRef>& named_types_,
		std::vector<ASTNodeRef>& top_level_defs_,
		int order_num_) 
		: i(i_), tokens(t), named_types(named_types_), top_level_defs(top_level_defs_), order_num(order_num_)/*, else_token_present(false)*/ {}
	const std::vector<Reference<TokenBase> >& tokens;
	//const char* text_buffer;
	//const std::string* text_buffer;
	const SourceBuffer* text_buffer;
	unsigned int& i;
	std::map<std::string, TypeVRef>& named_types;
	std::vector<ASTNodeRef>& top_level_defs; // Either function definitions or named constants.
	//bool else_token_present;
	int order_num;

	std::vector<std::string> generic_type_params; // Used when parsing types.
};


/*=====================================================================
LangParser
----------

=====================================================================*/
class LangParser
{
public:
	LangParser(bool floating_point_literals_default_to_double, bool real_is_double);

	~LangParser();

	Reference<BufferRoot> parseBuffer(const std::vector<Reference<TokenBase> >& tokens, 
		const SourceBufferRef& source_buffer,
		std::map<std::string, TypeVRef>& named_types,
		std::vector<TypeVRef>& named_types_ordered_out,
		int& function_order_num
	);

	static void test();

private:
	const std::string errorPosition(const SourceBuffer& buffer, unsigned int pos);
	const std::string errorPosition(const ParseInfo& parseinfo);
	const std::string errorPositionPrevToken(ParseInfo& parseinfo);

	const std::string parseIdentifier(const std::string& id_type, ParseInfo& parseinfo);
	void parseAndCheckIdentifier(const std::string& target_id, ParseInfo& parseinfo);
	ASTNodeRef parseLiteral(ParseInfo& parseinfo);
	Reference<IntLiteral> parseIntLiteral(ParseInfo& parseinfo);

	FunctionDefinitionRef parseFunctionDefinition(ParseInfo& parseinfo);
	FunctionDefinitionRef parseFunctionDefinitionGivenName(const std::string& func_name, ParseInfo& parseinfo, bool is_lambda);
	//Reference<ASTNode> parseFunctionDeclaration(const std::vector<Reference<TokenBase> >& tokens, const char* text_buffer, unsigned int& i);
	NamedConstantRef parseNamedConstant(ParseInfo& parseinfo);

	//Reference<ASTNode> parseFunctionExpression(ParseInfo& parseinfo);

	//void parseToken(const std::vector<Reference<TokenBase> >& tokens, const char* text_buffer, unsigned int token_type, const std::string& type_name, unsigned int& i);
	void parseToken(unsigned int token_type, ParseInfo& parseinfo);
	bool isTokenCurrent(unsigned int token_type, ParseInfo& parseinfo);
	void advance(ParseInfo& parseinfo);

	//ASTNodeRef parseFieldExpression(ParseInfo& parseinfo);
	ASTNodeRef parseVariableExpression(ParseInfo& parseinfo);
	ASTNodeRef parseIfExpression(ParseInfo& parseinfo);

	TypeVRef parseType(ParseInfo& parseinfo);
	TypeVRef parseSumType(ParseInfo& parseinfo);
	TypeVRef parseElementaryType(ParseInfo& parseinfo);
	TypeVRef parseMapType(ParseInfo& parseinfo);
	TypeVRef parseArrayType(ParseInfo& parseinfo);
	TypeVRef parseVArrayType(ParseInfo& parseinfo);
	TypeVRef parseFunctionType(ParseInfo& parseinfo);
	VRef<StructureType> parseStructType(ParseInfo& parseinfo);
	TypeVRef parseVectorType(ParseInfo& parseinfo);
	TypeVRef parseTupleType(ParseInfo& parseinfo);

	ASTNodeRef parseLetBlock(ParseInfo& parseinfo);
	ASTNodeRef parseExpression(ParseInfo& parseinfo);
	ASTNodeRef parseBasicExpression(ParseInfo& parseinfo);
	ASTNodeRef parseComparisonExpression(ParseInfo& parseinfo);
	ASTNodeRef parseUnaryExpression(ParseInfo& parseinfo);
	ASTNodeRef parseAddSubExpression(ParseInfo& parseinfo);
	ASTNodeRef parseMulDivExpression(ParseInfo& parseinfo);
	ASTNodeRef parseBinaryLogicalExpression(ParseInfo& parseinfo);
	ASTNodeRef parseShiftExpression(ParseInfo& parseinfo);

	ASTNodeRef parseBinaryBitwiseExpression(ParseInfo& parseinfo);
	//ASTNodeRef parseBitwiseOrExpression(ParseInfo& parseinfo);
	//ASTNodeRef parseBitwiseXorExpression(ParseInfo& parseinfo);
	//ASTNodeRef parseBitwiseAndExpression(ParseInfo& parseinfo);
	ASTNodeRef parseTernaryConditionalExpression(ParseInfo& parseinfo);
	//ASTNodeRef parseParenExpression(ParseInfo& parseinfo);
	ASTNodeRef parseMapLiteralExpression(ParseInfo& parseinfo);
	ASTNodeRef parseArrayOrVectorOrTupleLiteral(ParseInfo& parseinfo);
	//ASTNodeRef parseArraySubscriptExpression(ParseInfo& parseinfo);
	ASTNodeRef parseHighPrecedenceExpression(ParseInfo& parseinfo);
	Reference<LetASTNode> parseLet(ParseInfo& parseinfo);
	FunctionDefinitionRef parseAnonFunction(ParseInfo& parseinfo);
	void parseParameterList(ParseInfo& parseinfo, std::vector<FunctionDefinition::FunctionArg>& args_out);

	bool isKeyword(const std::string& name);
private:
	std::vector<unsigned int> comparison_tokens;
	bool floating_point_literals_default_to_double;
	bool real_is_double;
};


} // end namespace Winter
