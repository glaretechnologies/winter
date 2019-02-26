/*=====================================================================
LangParser.cpp
--------------
Copyright Glare Technologies Limited 2016 -
File created by ClassTemplate on Wed Jun 11 02:56:20 2008
=====================================================================*/
#include "wnt_LangParser.h"


#include "wnt_Lexer.h"
#include "wnt_ASTNode.h"
#include "wnt_FunctionExpression.h"
#include "wnt_IfExpression.h"
#include "wnt_Diagnostics.h"
#include "wnt_VectorLiteral.h"
#include "wnt_ArrayLiteral.h"
#include "wnt_TupleLiteral.h"
#include "wnt_VArrayLiteral.h"
#include "wnt_Variable.h"
#include "wnt_LetASTNode.h"
#include "wnt_LetBlock.h"
#include "BuiltInFunctionImpl.h"
#include "indigo/TestUtils.h"
#include "indigo/globals.h"
#include "maths/mathstypes.h"
#include "utils/StringUtils.h"
#include "utils/ContainerUtils.h"
#include "utils/Parser.h"
#include <assert.h>
#include <map>


using std::vector;
using std::string;


namespace Winter
{


LangParser::LangParser(bool floating_point_literals_default_to_double_, bool real_is_double_)
:	floating_point_literals_default_to_double(floating_point_literals_default_to_double_),
	real_is_double(real_is_double_)
{
	comparison_tokens.push_back(DOUBLE_EQUALS_TOKEN);
	comparison_tokens.push_back(NOT_EQUALS_TOKEN);
	comparison_tokens.push_back(LEFT_ANGLE_BRACKET_TOKEN);
	comparison_tokens.push_back(RIGHT_ANGLE_BRACKET_TOKEN);
	comparison_tokens.push_back(LESS_EQUAL_TOKEN);
	comparison_tokens.push_back(GREATER_EQUAL_TOKEN);
}


LangParser::~LangParser()
{
}


static const SrcLocation locationForParseInfo(ParseInfo& p)
{
	if(p.i >= p.tokens.size())
		return SrcLocation::invalidLocation();

	return SrcLocation(p.tokens[p.i]->char_index, p.text_buffer);
}


static const SrcLocation prevTokenLoc(ParseInfo& p)
{
	return SrcLocation(p.tokens[p.i - 1]->char_index, p.text_buffer);
}


Reference<BufferRoot> LangParser::parseBuffer(const std::vector<Reference<TokenBase> >& tokens, 
										   const SourceBufferRef& source_buffer,
										   std::map<std::string, TypeVRef>& named_types,
											std::vector<TypeVRef>& named_types_ordered_out,
											int& order_num)
{
	Reference<BufferRoot> root = new BufferRoot(SrcLocation(0, source_buffer.getPointer()));

	unsigned int i = 0;

	ParseInfo parseinfo(i, tokens, named_types, root->top_level_defs, order_num);
	parseinfo.text_buffer = source_buffer.getPointer();

	// NEW: go through buffer and see if there is a 'else' token
	/*for(size_t z=0; z<tokens.size(); ++z)
		if(tokens[z]->getType() == IDENTIFIER_TOKEN && tokens[z]->getIdentifierValue() == "else")
		{
			parseinfo.else_token_present = true;
			break;
		}*/

	while(i < tokens.size())
	{
		if(tokens[i]->isIdentifier() && tokens[i]->getIdentifierValue() == "def")
		{
			root->top_level_defs.push_back(parseFunctionDefinition(parseinfo));
			parseinfo.generic_type_params.clear();
			parseinfo.order_num++;
		}
		else if(tokens[i]->isIdentifier() && tokens[i]->getIdentifierValue() == "struct")
		{
			const unsigned int struct_position = parseinfo.i;
			VRef<StructureType> t = parseStructType(parseinfo);
				
			if(named_types.find(t->name) != named_types.end())
				throw BaseException("struct with name '" + t->name + "' already defined: " + errorPosition(*parseinfo.text_buffer, struct_position));

			named_types.insert(std::make_pair(t->name, t));
			named_types_ordered_out.push_back(t);

			// Make constructor function for this structure
			vector<FunctionDefinition::FunctionArg> args;
			args.reserve(t->component_types.size());
			for(unsigned int z=0; z<t->component_types.size(); ++z)
				args.push_back(FunctionDefinition::FunctionArg(t->component_types[z], t->component_names[z]));

			FunctionDefinitionRef cons = new FunctionDefinition(
				SrcLocation::invalidLocation(),
				parseinfo.order_num, // order number
				t->name, // name
				args, // arguments
				ASTNodeRef(), // body expr
				t, // declard return type
				new Constructor(t) // built in func impl.
			);
			root->top_level_defs.push_back(cons);

			// Make field access functions
			vector<FunctionDefinition::FunctionArg> getfield_args;
			getfield_args.push_back(FunctionDefinition::FunctionArg(t, "s"));

			for(unsigned int z=0; z<t->component_types.size(); ++z)
			{
				FunctionDefinitionRef def(new FunctionDefinition(
					SrcLocation::invalidLocation(),
					parseinfo.order_num, // order number
					t->component_names[z], // name
					getfield_args, // args
					ASTNodeRef(), // body expr
					t->component_types[z], // return type
					new GetField(t, z) // impl
				));

				root->top_level_defs.push_back(def);
			}

			parseinfo.order_num++;
		}
		else if(tokens[i]->isIdentifier())
		{
			// Parse named constant, e.g. "DOZEN = 12"
			root->top_level_defs.push_back(parseNamedConstant(parseinfo));
			parseinfo.order_num++;
		}
		else
			throw LangParserExcep("Expected 'def'." + errorPosition(*source_buffer, tokens[i]->char_index));
	}

	// Update order_num
	order_num = parseinfo.order_num;

	return root;
}


const std::string LangParser::parseIdentifier(const std::string& id_type, ParseInfo& p)
{
	if(p.i >= p.tokens.size())
		throw LangParserExcep("End of buffer before " + id_type + " identifier.");

	if(!p.tokens[p.i]->isIdentifier())
		throw LangParserExcep("Expected " + id_type + " identifier." + errorPosition(p));

	return p.tokens[p.i++]->getIdentifierValue();
}


void LangParser::parseAndCheckIdentifier(const std::string& target_id, ParseInfo& p)
{
	if(p.i >= p.tokens.size())
		throw LangParserExcep("End of buffer before " + target_id + " identifier.");

	if(!p.tokens[p.i]->isIdentifier())
		throw LangParserExcep("Expected identifier '" + target_id + "'." + errorPosition(p));

	if(p.tokens[p.i]->getIdentifierValue() != target_id)
		throw LangParserExcep("Expected identifier '" + target_id + "'." + errorPosition(p));

	p.i++;
}


// TODO: this should probably be a virtual method on TokenBase.
static const std::string tokenDescription(const Reference<TokenBase>& token)
{
	switch(token->getType())
	{
	case FLOAT_LITERAL_TOKEN:
		return "float literal '" + toString(token->getFloatLiteralValue()) + "'";
	case INT_LITERAL_TOKEN:
		return "int literal '" + toString(token->getIntLiteralValue()) + "'";
	case BOOL_LITERAL_TOKEN:
		return "bool literal '" + boolToString(token->getBoolLiteralValue()) + "'";
	case STRING_LITERAL_TOKEN:
		return "string literal '" + token->getStringLiteralValue() + "'";
	case CHAR_LITERAL_TOKEN:
		return "char literal '" + token->getCharLiteralValue() + "'";
	case IDENTIFIER_TOKEN:
		return "identifier '" + token->getIdentifierValue() + "'";
	default:
		return tokenName(token->getType());
	};
}


void LangParser::parseToken(unsigned int token_type, ParseInfo& p)
{
	if(p.i >= p.tokens.size())
		throw LangParserExcep("End of buffer before " + tokenName(token_type) + " token.");
	
	if(p.tokens[p.i]->getType() != token_type)
		throw LangParserExcep("Expected " + tokenName(token_type) + ", found " + tokenDescription(p.tokens[p.i]) + "." + errorPosition(p));

	p.i++;
}


void LangParser::skipExpectedToken(unsigned int token_type, ParseInfo& p)
{
	assert(isTokenCurrent(token_type, p));
	p.i++;
}


bool LangParser::isTokenCurrent(unsigned int token_type, ParseInfo& p)
{
	return p.i < p.tokens.size() && p.tokens[p.i]->getType() == token_type;
}


void LangParser::advance(ParseInfo& p)
{
	p.i++;
}


/*ASTNodeRef LangParser::parseFieldExpression(ParseInfo& p)
{
	ASTNodeRef left = parseArraySubscriptExpression(p);
	
	while(isTokenCurrent(DOT_TOKEN, p))
	{
		SrcLocation src_loc = locationForParseInfo(p);

		parseToken(DOT_TOKEN, p);

		const std::string field_name = parseIdentifier("field name", p);

		FunctionExpressionRef func_expr = new FunctionExpression(src_loc);
		func_expr->function_name = field_name;
		func_expr->argument_expressions.push_back(left);
		left = func_expr;
	}

	return left;
}*/


#if 0
ASTNodeRef LangParser::parseFieldExpression(ParseInfo& p)
{
	if(p.i >= p.tokens.size())
		throw LangParserExcep("End of buffer while parsing field expression.");

	ASTNodeRef var_expression;

	if(p.tokens[p.i]->getType() == IDENTIFIER_TOKEN && p.tokens[p.i]->getIdentifierValue() == "if")
	{
		var_expression = parseIfExpression(p);
	}
	else
	{
		// If next token is a '(', then this is a function expression
		if(p.i + 1 < p.tokens.size() && p.tokens[p.i+1]->getType() == OPEN_PARENTHESIS_TOKEN)
			var_expression = parseFunctionExpression(p);
		else
		{
			var_expression = parseVariableExpression(p);
		}
	}

	while(isTokenCurrent(DOT_TOKEN, p))
	{
		SrcLocation src_loc = locationForParseInfo(p);

		skipExpectedToken(DOT_TOKEN, p);

		const std::string field_name = parseIdentifier("field name", p);

		FunctionExpressionRef func_expr = new FunctionExpression(src_loc);
		func_expr->function_name = field_name;
		func_expr->argument_expressions.push_back(var_expression);
		var_expression = func_expr;
	}

/*

a.b.c
->
c(b(a))

->
c
|
b
|
a


*/

	return var_expression;
}
#endif
		
/*
There are several forms of if to parse:
New form with optional 'then':
if a then b else c
if a b else c

old form:
if(a, b, c)


New form may also happen to have parens at the start of condition expression:
if (x < 5) then b else c
if (x * 2) < 5 then b else c

*/
ASTNodeRef LangParser::parseIfExpression(ParseInfo& p)
{
	const SrcLocation loc = locationForParseInfo(p);

	parseAndCheckIdentifier("if", p);


	if(p.i >= p.tokens.size())
		throw LangParserExcep("End of buffer while parsing if expression.");

	if(p.tokens[p.i]->getType() == OPEN_PARENTHESIS_TOKEN)
	{
		unsigned int open_paren_pos = p.i;

		skipExpectedToken(OPEN_PARENTHESIS_TOKEN, p);

		// We are either parsing an old form of if: 'if(a, b, c)', or the new form with the condition expression in parens: 'if (a) then b else c' or 'if (a_0) binop a_1 then b else c'
		// We can distinguish the two by parsing the condition 'a', then seeing if the next token is ','.

		// Parse condition
		ASTNodeRef condition = parseExpression(p);

		if(p.i >= p.tokens.size())
			throw LangParserExcep("End of buffer while parsing if expression.");

		if(p.tokens[p.i]->getType() == COMMA_TOKEN)
		{
			// We are parsing the old form of if.
			p.i++; // Advance past ','.

			// Parse then expression
			ASTNodeRef then_expr = parseExpression(p);

			parseToken(COMMA_TOKEN, p);
	
			// Parse else expression
			ASTNodeRef else_expr = parseExpression(p);
		
			parseToken(CLOSE_PARENTHESIS_TOKEN, p);

			return new IfExpression(loc, condition, then_expr, else_expr);
		}
		else
		{
			// We are parsing the new form of if.  
			// Go back and parse condition expression again.
			p.i = open_paren_pos;
			assert(p.tokens[p.i]->getType() == OPEN_PARENTHESIS_TOKEN);
			
			// Parse condition
			condition = parseExpression(p);

			// Parse optional 'then'
			if(p.i < p.tokens.size() && p.tokens[p.i]->isIdentifier() && p.tokens[p.i]->getIdentifierValue() == "then")
				parseAndCheckIdentifier("then", p);


			// Parse then expression
			ASTNodeRef then_expr = parseExpression(p);

			// Parse mandatory 'else'
			parseAndCheckIdentifier("else", p);

			// Parse else expression
			ASTNodeRef else_expr = parseExpression(p);

			return new IfExpression(loc, condition, then_expr, else_expr);
		}
	}
	else
	{
		// No opening '(', so we are parsing the new form of if.

		// Parse condition
		ASTNodeRef condition = parseExpression(p);

		// Parse optional 'then'
		if(p.i < p.tokens.size() && p.tokens[p.i]->isIdentifier() && p.tokens[p.i]->getIdentifierValue() == "then")
			parseAndCheckIdentifier("then", p);

		// Parse then expression
		ASTNodeRef then_expr = parseExpression(p);

		// Parse mandatory 'else'
		parseAndCheckIdentifier("else", p);

		// Parse else expression
		ASTNodeRef else_expr = parseExpression(p);

		return new IfExpression(loc, condition, then_expr, else_expr);
	}
}


ASTNodeRef LangParser::parseVariableExpression(ParseInfo& p)
{
	const SrcLocation loc = locationForParseInfo(p);

	const std::string name = parseIdentifier("variable name", p);
	if(isKeyword(name))
		throw LangParserExcep("Cannot call a variable '" + name + "' - is a keyword.  " +  errorPositionPrevToken(p));

	return new Variable(name, loc);
}


bool LangParser::isKeyword(const std::string& name)
{
	return 
		name == "let" ||
		name == "def" ||
		name == "in" ||
		name == "fn";
	// TODO: finish
}


Reference<FunctionDefinition> LangParser::parseFunctionDefinition(ParseInfo& p)
{
	parseAndCheckIdentifier("def", p);

	const std::string function_name = parseIdentifier("function name", p);

	return parseFunctionDefinitionGivenName(function_name, p, /*is_lambda=*/false);
}


FunctionDefinitionRef LangParser::parseFunctionDefinitionGivenName(const std::string& func_name, ParseInfo& p, bool is_lambda)
{
	try
	{
		SrcLocation loc = prevTokenLoc(p);

		// Parse generic parameters, if present
		std::vector<std::string> generic_type_param_names;
		p.generic_type_params.resize(0);
		if(isTokenCurrent(LEFT_ANGLE_BRACKET_TOKEN, p))
		{
			skipExpectedToken(LEFT_ANGLE_BRACKET_TOKEN, p);

			const std::string type_param_name = parseIdentifier("type parameter", p);
			generic_type_param_names.push_back(type_param_name);
			p.generic_type_params.push_back(type_param_name);

			while(isTokenCurrent(COMMA_TOKEN, p))
			{
				skipExpectedToken(COMMA_TOKEN, p);
				const std::string type_param_name2 = parseIdentifier("type parameter", p);
				generic_type_param_names.push_back(type_param_name2);
				p.generic_type_params.push_back(type_param_name2);
			}

			parseToken(RIGHT_ANGLE_BRACKET_TOKEN, p);
		}


		// Parse parameter list
		std::vector<FunctionDefinition::FunctionArg> args;
		parseParameterList(p, args);

		// Fill in generic_type_param_index for all generic types
		//for(unsigned int i=0; i<args.size(); ++i)
		//	for(unsigned int z=0; z<generic_type_params.size(); ++z)
		//		if(generic_type_params[z] == args[i].
		
		//parseToken(tokens, text_buffer, Token::RIGHT_ARROW, i);

		// Parse function attributes.
		bool noinline = false;
		if(isTokenCurrent(EXCLAMATION_MARK_TOKEN, p))
		{
			skipExpectedToken(EXCLAMATION_MARK_TOKEN, p);

			const std::string attribute = parseIdentifier("attribute", p);
			if(attribute == "noinline")
			{
				noinline = true;
			}
			else throw LangParserExcep("Error occurred while parsing function '" + func_name + "': unknown attribute '" + attribute + "'" +  errorPosition(p));
		}


		// Parse optional return type
		TypeRef return_type(NULL);

		if(is_lambda)
		{
			// Both ':' and '->' are acceptable after the arg list
			if(!(isTokenCurrent(COLON_TOKEN, p) || isTokenCurrent(RIGHT_ARROW_TOKEN, p)))
				return_type = parseType(p);
		
			if(isTokenCurrent(COLON_TOKEN, p))
				skipExpectedToken(COLON_TOKEN, p);
			else if(isTokenCurrent(RIGHT_ARROW_TOKEN, p))
				skipExpectedToken(RIGHT_ARROW_TOKEN, p);
			else
				throw LangParserExcep("Error occurred while parsing anon function: expected ':' or '->'" + errorPosition(p));
		}
		else
		{
			if(!isTokenCurrent(COLON_TOKEN, p))
				return_type = parseType(p);
		
			parseToken(COLON_TOKEN, p);
		}
		
		// Parse function body
		ASTNodeRef body = parseExpression(p);

		Reference<FunctionDefinition> def = new FunctionDefinition(
			loc,
			p.order_num,
			func_name,
			args,
			body,
			return_type, // declared return type
			NULL // built in func impl
		);
		def->generic_type_param_names = generic_type_param_names;
		def->noinline = noinline;

		return def;
	}
	catch(LangParserExcep& e)
	{
		throw LangParserExcep("Error occurred while parsing function '" + func_name + "': " + e.what());
	}
}


NamedConstantRef LangParser::parseNamedConstant(ParseInfo& p)
{
	const SrcLocation src_loc = locationForParseInfo(p);

	const unsigned int initial_pos = p.i;

	std::string name = parseIdentifier("name", p);

	TypeRef declared_type;

	if(!isTokenCurrent(EQUALS_TOKEN, p))
	{
		// Then assume what we parsed was the optional type.  So backtrack and re-parse
		p.i = initial_pos; // backtrack
		declared_type = parseType(p);

		name = parseIdentifier("variable name", p);
	}

	parseToken(EQUALS_TOKEN, p);

	const ASTNodeRef value_expr = parseExpression(p);

	return new NamedConstant(declared_type, name, value_expr, src_loc, p.order_num);
}


/*Reference<ASTNode> LangParser::parseFunctionExpression(ParseInfo& p)
{
	SrcLocation src_loc = locationForParseInfo(p);

	const std::string func_name = parseIdentifier("function name", p);

	// Parse parameter list
	parseToken(OPEN_PARENTHESIS_TOKEN, p);

	if(p.i == p.tokens.size())
		throw LangParserExcep("Expected ')'");

	std::vector<Reference<ASTNode> > arg_expressions;

	FunctionExpressionRef expr = new FunctionExpression(src_loc);

	if(p.tokens[p.i]->getType() != CLOSE_PARENTHESIS_TOKEN)
	{
		arg_expressions.push_back(parseExpression(p));
	}

	while(p.i < p.tokens.size() && p.tokens[p.i]->getType() != CLOSE_PARENTHESIS_TOKEN)//isTokenCurrent(CLOSE_PARENTHESIS_TOKEN, p)) //p.tokens[p.i]->getType() != CLOSE_PARENTHESIS_TOKEN)
	{
		parseToken(COMMA_TOKEN, p);
	
		arg_expressions.push_back(parseExpression(p));
	}

	parseToken(CLOSE_PARENTHESIS_TOKEN, p);

	expr->argument_expressions = arg_expressions;
	expr->function_name = func_name;
	return expr;
}*/


/*
Reference<ASTNode> LangParser::parseFunctionDeclaration(const std::vector<Reference<TokenBase> >& tokens, const char* text_buffer, unsigned int& i)
{
	Reference<ASTNode> node( new ASTNode(ASTNode::FUNCTION_DECLARATION) );

	parseIdentifier("declare", tokens, text_buffer, i);

	node->function_def_name = parseIdentifier("function name", tokens, text_buffer, i);

	// Parse parameter list
	parseToken(tokens, text_buffer, OPEN_PARENTHESIS_TOKEN, i);

	while(1)
	{
		const std::string param_type = parseIdentifier("parameter type", tokens, text_buffer, i);
		const std::string param_name = parseIdentifier("parameter name", tokens, text_buffer, i);

		node->function_def_args.push_back(Argument());
		node->function_def_args.back().type = param_type;
		node->function_def_args.back().name = param_name;
		
		if(i >= tokens.size())
		{
			throw LangParserExcep("End of buffer before end of parameter list.");
		}
		else if(tokens[i].type == Token::CLOSE_PARENTHESIS)
		{
			i++;
			break;
		}
		else if(tokens[i].type == Token::COMMA)
		{
			i++;
		}
		else
		{
			throw LangParserExcep("Expected ',' or ')' while parsing parameter list of function '" + node->function_def_name + "'" + errorPosition(text_buffer, tokens[i].char_index));
		}
	}

	//parseToken(tokens, text_buffer, Token::RIGHT_ARROW, i);

	// Parse return type
	node->function_def_return_type = parseIdentifier("return type", tokens, text_buffer, i);

	return node;
}*/


ASTNodeRef LangParser::parseLiteral(ParseInfo& p)
{
	SrcLocation loc = locationForParseInfo(p);

	if(p.i >= p.tokens.size())
		throw LangParserExcep("End of buffer while parsing literal." + errorPosition(p));

	if(p.tokens[p.i]->getType() == INT_LITERAL_TOKEN)
	{
		const IntLiteralToken* token = static_cast<const IntLiteralToken*>(p.tokens[p.i].getPointer());
		ASTNodeRef n = new IntLiteral(token->getIntLiteralValue(), token->num_bits, token->is_signed, loc);
		p.i++;
		return n;
	}
	else if(p.tokens[p.i]->getType() == FLOAT_LITERAL_TOKEN)
	{
		if(static_cast<FloatLiteralToken*>(p.tokens[p.i].getPointer())->suffix == 'f')
			return new FloatLiteral((float)p.tokens[p.i++]->getFloatLiteralValue(), loc);
		else if(static_cast<FloatLiteralToken*>(p.tokens[p.i].getPointer())->suffix == 'd')
			return new DoubleLiteral(p.tokens[p.i++]->getFloatLiteralValue(), loc);
		else
		{
			// no suffix:
			if(floating_point_literals_default_to_double)
				return new DoubleLiteral(p.tokens[p.i++]->getFloatLiteralValue(), loc);
			else
				return new FloatLiteral((float)p.tokens[p.i++]->getFloatLiteralValue(), loc);
		}
	}
	else if(p.tokens[p.i]->getType() == STRING_LITERAL_TOKEN)
	{
		return new StringLiteral(p.tokens[p.i++]->getStringLiteralValue(), loc);
	}
	else if(p.tokens[p.i]->getType() == CHAR_LITERAL_TOKEN)
	{
		return new CharLiteral(p.tokens[p.i++]->getCharLiteralValue(), loc);
	}
	else if(p.tokens[p.i]->getType() == BOOL_LITERAL_TOKEN)
	{
		return new BoolLiteral(p.tokens[p.i++]->getBoolLiteralValue(), loc);
	}
	else
	{
		throw LangParserExcep("token is not a literal" + errorPosition(p));
	}
}


Reference<IntLiteral> LangParser::parseIntLiteral(ParseInfo& p)
{
	SrcLocation loc = locationForParseInfo(p);

	if(p.i >= p.tokens.size())
		throw LangParserExcep("End of buffer while parsing int literal." + errorPosition(p));

	if(p.tokens[p.i]->getType() == INT_LITERAL_TOKEN)
	{
		const IntLiteralToken* token = static_cast<const IntLiteralToken*>(p.tokens[p.i].getPointer());
		Reference<IntLiteral> n = new IntLiteral(token->getIntLiteralValue(), token->num_bits, token->is_signed, loc);
		p.i++;
		return n;
	}
	else
	{
		throw LangParserExcep("token is not an integer literal." + errorPosition(p));
	}
}


Reference<ASTNode> LangParser::parseLetBlock(ParseInfo& p)
{
	if(p.i < p.tokens.size() && p.tokens[p.i]->isIdentifier() && p.tokens[p.i]->getIdentifierValue() == "let")
	{
		SrcLocation loc = locationForParseInfo(p);

		p.i++;

		vector<Reference<LetASTNode> > lets;
		
		while(p.i < p.tokens.size() && !(p.tokens[p.i]->isIdentifier() && p.tokens[p.i]->getIdentifierValue() == "in"))
		{
			const unsigned int let_position = p.i;
			Reference<LetASTNode> let = parseLet(p);

			// Before we add it, go back over the other lets in the let block to make sure this name is unique.
			for(size_t z=0; z<lets.size(); ++z)
				for(size_t w=0; w<let->vars.size(); ++w)
					for(size_t t=0; t<lets[z]->vars.size(); ++t)
						if(lets[z]->vars[t].name == let->vars[w].name)
							throw LangParserExcep("Let with this name already defined in let block." + errorPosition(*p.text_buffer, p.tokens[let_position]->char_index));

			lets.push_back(let);
		}

		parseAndCheckIdentifier("in", p);

		ASTNodeRef main_expr = parseExpression(p);

		return new LetBlock(main_expr, lets, loc);
	}


	return parseTernaryConditionalExpression(p);
}


ASTNodeRef LangParser::parseExpression(ParseInfo& p)
{
	return parseLetBlock(p);
}


// A basic expression is a literal, or a variable, or an if expression
ASTNodeRef LangParser::parseBasicExpression(ParseInfo& p)
{
	if(p.i >= p.tokens.size())
		throw LangParserExcep("End of buffer while parsing basic expression." + errorPosition(p));

	SrcLocation loc = locationForParseInfo(p);

	if(isTokenCurrent(OPEN_PARENTHESIS_TOKEN, p))
	{
		// Parse parenthesised expression
		p.i++; // Comsume open paren

		if(isTokenCurrent(CLOSE_PARENTHESIS_TOKEN, p))
		{
			// Then this is an empty tuple, which we won't allow
			throw LangParserExcep("Empty tuples not allowed." + errorPosition(p));
		}

		const ASTNodeRef e = parseExpression(p);

		if(isTokenCurrent(COMMA_TOKEN, p)) // If there is a comma here, we are parsing a tuple, e.g. "(1, 2)"
		{
			p.i++; // Consume comma

			vector<ASTNodeRef> tuple_elems(1, e);
			while(1)
			{
				tuple_elems.push_back(parseExpression(p));

				if(isTokenCurrent(CLOSE_PARENTHESIS_TOKEN, p))
				{
					// done.
					p.i++;
					return new TupleLiteral(tuple_elems, loc);
				}
				else if(isTokenCurrent(COMMA_TOKEN, p))
				{
					p.i++;
				}
				else
					throw LangParserExcep("Unexpected token while parsing tuple." + errorPosition(p));
			}
		}

		parseToken(CLOSE_PARENTHESIS_TOKEN, p);

		return e;
	}
	else if(p.tokens[p.i]->isLiteral())
	{
		return parseLiteral(p);
	}
	else if(p.tokens[p.i]->isIdentifier())
	{
		if(p.tokens[p.i]->getIdentifierValue() == "if")
			return parseIfExpression(p);
		else
			return parseVariableExpression(p);
	}
	/*TEMP else if(p.tokens[p.i]->getType() == OPEN_BRACE_TOKEN)
	{
		return parseMapLiteralExpression(p);
	}*/
	else if(p.tokens[p.i]->getType() == OPEN_SQUARE_BRACKET_TOKEN)
	{
		return parseArrayOrVectorOrTupleLiteral(p);
	}
	else if(p.tokens[p.i]->getType() == BACK_SLASH_TOKEN)
	{
		return parseAnonFunction(p);
	}
	else
	{
		throw LangParserExcep("Expected literal or identifier in expression." + errorPosition(p));
	}
}


TypeVRef LangParser::parseSumType(ParseInfo& p)
{
	TypeVRef t = parseElementaryType(p);

	if(!isTokenCurrent(OR_TOKEN, p))
		return t;
	
	vector<TypeVRef> types(1, t);

	while(isTokenCurrent(OR_TOKEN, p))
	{
		skipExpectedToken(OR_TOKEN, p);

		types.push_back(parseElementaryType(p));
	}

	return new SumType(types);
}


TypeVRef LangParser::parseType(ParseInfo& p)
{
	return parseSumType(p);
}


TypeVRef LangParser::parseElementaryType(ParseInfo& p)
{
	std::string t = parseIdentifier("type", p);

	// Handle optional address space qualifier for type.
	std::string address_space;
	if(t == "constant" || t == "global" || t == "__constant" || t == "__global")
	{
		address_space = t;
		t = parseIdentifier("type", p);
	}

	if(t == "float")
		return new Float();
	else if(t == "double")
		return new Double();
	else if(t == "real")
	{
		if(real_is_double)
			return new Double();
		else
			return new Float();
	}
	else if(t == "int")
		return new Int(32);
	else if(t == "int16")
		return new Int(16);
	else if(t == "int32")
		return new Int(32);
	else if(t == "int64")
		return new Int(64);

	else if(t == "uint")
		return new Int(32, /*signed=*/false);
	else if(t == "uint16")
		return new Int(16, /*signed=*/false);
	else if(t == "uint32")
		return new Int(32, /*signed=*/false);
	else if(t == "uint64")
		return new Int(64, /*signed=*/false);

	else if(t == "string")
		return new String();
	else if(t == "char")
		return new CharType();
	else if(t == "opaque" || t == "voidptr")
	{
		TypeVRef the_type = new OpaqueType();
		the_type->address_space = address_space;
		return the_type;
	}
	else if(t == "bool")
		return new Bool();
	//else if(t == "error")
	//	return new ErrorType();
	else if(t == "map")
		return parseMapType(p);
	else if(t == "array")
	{
		TypeVRef the_type = parseArrayType(p);
		the_type->address_space = address_space;
		return the_type;
	}
	else if(t == "varray")
		return parseVArrayType(p);
	else if(t == "function")
		return parseFunctionType(p);
	else if(t == "vector")
		return parseVectorType(p);
	else if(t == "tuple")
		return parseTupleType(p);
	else
	{
		// Then this might be the name of a named type.
		// So look up the named type map
		std::map<std::string, TypeVRef>::const_iterator res = p.named_types.find(t);
		if(res == p.named_types.end())
		{
			// Not a named type, maybe it is a type parameter
			for(unsigned int i=0; i<p.generic_type_params.size(); ++i)
				if(t == p.generic_type_params[i])
					return new GenericType(t, i);

			// If it wasn't a generic type, then it's completely unknown, like a rolling stone.
			//throw LangParserExcep("Unknown type '" + t + "'." + errorPositionPrevToken(p));
			TypeVRef the_type = new OpaqueStructureType(t);
			the_type->address_space = address_space;
			return the_type;
		}
		else
		{
			// Type found, return it
			(*res).second->address_space = address_space; // TEMP HACK
			return (*res).second;
		}
	}
}


TypeVRef LangParser::parseMapType(ParseInfo& p)
{
	parseToken(LEFT_ANGLE_BRACKET_TOKEN, p);

	TypeVRef from = parseType(p);

	parseToken(COMMA_TOKEN, p);

	TypeVRef to = parseType(p);

	parseToken(RIGHT_ANGLE_BRACKET_TOKEN, p);

	return new Map(from, to);
}


TypeVRef LangParser::parseArrayType(ParseInfo& p)
{
	parseToken(LEFT_ANGLE_BRACKET_TOKEN, p);

	TypeVRef t = parseType(p);

	parseToken(COMMA_TOKEN, p);

	Reference<IntLiteral> int_literal = parseIntLiteral(p);

	parseToken(RIGHT_ANGLE_BRACKET_TOKEN, p);

	return new ArrayType(t, int_literal->value);
}


TypeVRef LangParser::parseVArrayType(ParseInfo& p)
{
	parseToken(LEFT_ANGLE_BRACKET_TOKEN, p);

	TypeVRef t = parseType(p);

	parseToken(RIGHT_ANGLE_BRACKET_TOKEN, p);

	return new VArrayType(t);
}


TypeVRef LangParser::parseFunctionType(ParseInfo& p)
{
	parseToken(LEFT_ANGLE_BRACKET_TOKEN, p);

	std::vector<TypeVRef> types;

	types.push_back(parseType(p));

	while(isTokenCurrent(COMMA_TOKEN, p))
	{
		skipExpectedToken(COMMA_TOKEN, p);

		types.push_back(parseType(p));
	}

	parseToken(RIGHT_ANGLE_BRACKET_TOKEN, p);

	std::vector<TypeVRef> arg_types;
	for(int i=0; i<(int)types.size() - 1; ++i)
		arg_types.push_back(types[i]);

	return new Function(
		arg_types, 
		types.back(), 
		//vector<TypeRef>(), // captured var types
		false // use_captured_vars
	);
}


VRef<StructureType> LangParser::parseStructType(ParseInfo& p)
{
	parseAndCheckIdentifier("struct", p);

	const std::string name = parseIdentifier("structure name", p);

	parseToken(OPEN_BRACE_TOKEN, p);

	std::vector<TypeVRef> types;
	std::vector<string> names;

	if(!isTokenCurrent(CLOSE_BRACE_TOKEN, p))
	{
		types.push_back(parseType(p));
		names.push_back(parseIdentifier("field name", p));

		while(isTokenCurrent(COMMA_TOKEN, p))
		{
			skipExpectedToken(COMMA_TOKEN, p);

			types.push_back(parseType(p));
			names.push_back(parseIdentifier("field name", p));
		}
	}

	parseToken(CLOSE_BRACE_TOKEN, p);

	return new StructureType(name, types, names);
}


TypeVRef LangParser::parseVectorType(ParseInfo& p)
{
	parseToken(LEFT_ANGLE_BRACKET_TOKEN, p);

	TypeVRef t = parseType(p);

	parseToken(COMMA_TOKEN, p);

	Reference<IntLiteral> int_literal = parseIntLiteral(p);
	const int64 num = int_literal->value;

	if(num <= 0 || num >= 128)
		throw LangParserExcep("num must be > 0, < 128");

	parseToken(RIGHT_ANGLE_BRACKET_TOKEN, p);

	return new VectorType(t, (int)num);
}


TypeVRef LangParser::parseTupleType(ParseInfo& p)
{
	parseToken(LEFT_ANGLE_BRACKET_TOKEN, p);

	std::vector<TypeVRef> types;

	types.push_back(parseType(p));

	while(isTokenCurrent(COMMA_TOKEN, p))
	{
		skipExpectedToken(COMMA_TOKEN, p);

		types.push_back(parseType(p));
	}

	parseToken(RIGHT_ANGLE_BRACKET_TOKEN, p);

	return new TupleType(types);
}


ASTNodeRef LangParser::parseAddSubExpression(ParseInfo& p)
{
/*
Should be left associative
a + b + c = (a + b) + c


     +
    / \
   +   c
  / \
 a   b


*/
	ASTNodeRef left = parseMulDivExpression(p);

	while(1)
	{
		SrcLocation loc = locationForParseInfo(p);

		if(isTokenCurrent(PLUS_TOKEN, p))
		{
			skipExpectedToken(PLUS_TOKEN, p);

			Reference<AdditionExpression> addexpr = new AdditionExpression(loc, left, parseMulDivExpression(p));
			
			left = addexpr;
		}
		else if(isTokenCurrent(MINUS_TOKEN, p))
		{
			skipExpectedToken(MINUS_TOKEN, p);

			Reference<SubtractionExpression> e = new SubtractionExpression(loc, left, parseMulDivExpression(p));
			
			left = e;
		}
		else
		{
			return left;
		}
	}
}


ASTNodeRef LangParser::parseMulDivExpression(ParseInfo& p)
{
	ASTNodeRef left = parseUnaryExpression(p);
	while(1)
	{
		SrcLocation loc = locationForParseInfo(p);

		if(isTokenCurrent(ASTERISK_TOKEN, p))
		{
			skipExpectedToken(ASTERISK_TOKEN, p);

			Reference<MulExpression> expr = new MulExpression(loc, left, parseUnaryExpression(p));
			left = expr;
		}
		else if(isTokenCurrent(FORWARDS_SLASH_TOKEN, p))
		{
			skipExpectedToken(FORWARDS_SLASH_TOKEN, p);

			Reference<DivExpression> expr = new DivExpression(loc, left, parseUnaryExpression(p));
			left = expr;
		}
		else
			return left;
	}
}


ASTNodeRef LangParser::parseBinaryLogicalExpression(ParseInfo& p)
{
	ASTNodeRef left = parseBinaryBitwiseExpression(p);
	while(1)
	{
		SrcLocation loc = locationForParseInfo(p);

		if(isTokenCurrent(AND_TOKEN, p))
		{
			skipExpectedToken(AND_TOKEN, p);

			left = new BinaryBooleanExpr(
				BinaryBooleanExpr::AND,
				left,
				parseBinaryBitwiseExpression(p),
				loc
			);
		}
		else if(isTokenCurrent(OR_TOKEN, p))
		{
			skipExpectedToken(OR_TOKEN, p);

			left = new BinaryBooleanExpr(
				BinaryBooleanExpr::OR,
				left,
				parseBinaryBitwiseExpression(p),
				loc
			);
		}
		else
			return left;
	}
}


ASTNodeRef LangParser::parseShiftExpression(ParseInfo& p)
{
	ASTNodeRef left = parseAddSubExpression(p);
	while(1)
	{
		SrcLocation loc = locationForParseInfo(p);

		if(isTokenCurrent(LEFT_SHIFT_TOKEN, p))
		{
			skipExpectedToken(LEFT_SHIFT_TOKEN, p);

			left = new BinaryBitwiseExpression(
				BinaryBitwiseExpression::BITWISE_LEFT_SHIFT,
				left,
				parseAddSubExpression(p),
				loc
			);
		}
		else if(isTokenCurrent(RIGHT_SHIFT_TOKEN, p))
		{
			skipExpectedToken(RIGHT_SHIFT_TOKEN, p);

			left = new BinaryBitwiseExpression(
				BinaryBitwiseExpression::BITWISE_RIGHT_SHIFT,
				left,
				parseAddSubExpression(p),
				loc
			);
		}
		else
			return left;
	}
}



ASTNodeRef LangParser::parseBinaryBitwiseExpression(ParseInfo& p)
{
	ASTNodeRef left = parseComparisonExpression(p);
	while(1)
	{
		SrcLocation loc = locationForParseInfo(p);

		if(isTokenCurrent(BITWISE_AND_TOKEN, p))
		{
			skipExpectedToken(BITWISE_AND_TOKEN, p);

			left = new BinaryBitwiseExpression(
				BinaryBitwiseExpression::BITWISE_AND,
				left,
				parseComparisonExpression(p),
				loc
			);
		}
		else if(isTokenCurrent(BITWISE_OR_TOKEN, p))
		{
			skipExpectedToken(BITWISE_OR_TOKEN, p);

			left = new BinaryBitwiseExpression(
				BinaryBitwiseExpression::BITWISE_OR,
				left,
				parseComparisonExpression(p),
				loc
			);
		}
		else if(isTokenCurrent(BITWISE_XOR_TOKEN, p))
		{
			skipExpectedToken(BITWISE_XOR_TOKEN, p);

			left = new BinaryBitwiseExpression(
				BinaryBitwiseExpression::BITWISE_XOR,
				left,
				parseComparisonExpression(p),
				loc
			);
		}
		else
			return left;
	}
}


ASTNodeRef LangParser::parseTernaryConditionalExpression(ParseInfo& p)
{
	ASTNodeRef left = parseBinaryLogicalExpression(p);

	if(isTokenCurrent(QUESTION_MARK_TOKEN, p))
	{
		skipExpectedToken(QUESTION_MARK_TOKEN, p);

		ASTNodeRef then_expr = parseTernaryConditionalExpression(p);

		parseToken(COLON_TOKEN, p); // Parse ':'.

		ASTNodeRef else_expr = parseTernaryConditionalExpression(p);

		return new IfExpression(left->srcLocation(),
			left,
			then_expr,
			else_expr
		);
	}
	else
		return left;
}


ASTNodeRef LangParser::parseComparisonExpression(ParseInfo& p)
{
	ASTNodeRef left = parseShiftExpression(p);

	SrcLocation loc = locationForParseInfo(p);

	for(unsigned int i=0; i<comparison_tokens.size(); ++i)
	{
		const unsigned int token = comparison_tokens[i];
		if(isTokenCurrent(token, p))
		{
			skipExpectedToken(token, p);

			Reference<TokenBase> token_ref = makeTokenObject(token, p.tokens[p.i - 1]->char_index);

			Reference<ComparisonExpression> expr = new ComparisonExpression(
				token_ref,
				left, 
				parseShiftExpression(p),
				loc
			);
			return expr;
		}
	}

	return left;
}


ASTNodeRef LangParser::parseUnaryExpression(ParseInfo& p)
{
	if(isTokenCurrent(MINUS_TOKEN, p))
	{
		SrcLocation loc = locationForParseInfo(p);

		parseToken(MINUS_TOKEN, p);

		return new UnaryMinusExpression(loc, parseUnaryExpression(p));
	}
	else if(isTokenCurrent(EXCLAMATION_MARK_TOKEN, p))
	{
		SrcLocation loc = locationForParseInfo(p);

		parseToken(EXCLAMATION_MARK_TOKEN, p);

		return new LogicalNegationExpr(loc, parseUnaryExpression(p));
	}
	else
	{
		return parseHighPrecedenceExpression(p);
	}
}


ASTNodeRef LangParser::parseHighPrecedenceExpression(ParseInfo& p)
{
	ASTNodeRef left = parseBasicExpression(p);

	while(1)
	{
		SrcLocation loc = locationForParseInfo(p);

		const unsigned int initial_pos = p.i;
		
		if(isTokenCurrent(OPEN_PARENTHESIS_TOKEN, p))
		{
			// Parse function call
			p.i++; // Skip OPEN_PARENTHESIS_TOKEN

			// Parse parameter list
			if(p.i == p.tokens.size())
				throw LangParserExcep("Expected ')'");

			vector<ASTNodeRef> arg_expressions;

			FunctionExpressionRef func_expr = new FunctionExpression(loc);

			if(p.tokens[p.i]->getType() != CLOSE_PARENTHESIS_TOKEN)
			{
				arg_expressions.push_back(parseExpression(p));
			}

			while(p.i < p.tokens.size() && p.tokens[p.i]->getType() != CLOSE_PARENTHESIS_TOKEN)
			{
				parseToken(COMMA_TOKEN, p);
	
				arg_expressions.push_back(parseExpression(p));
			}

			parseToken(CLOSE_PARENTHESIS_TOKEN, p);

			func_expr->get_func_expr = left;
			func_expr->argument_expressions = arg_expressions;
			left = func_expr;
		}
		else if(isTokenCurrent(OPEN_SQUARE_BRACKET_TOKEN, p))
		{
			// Parse subscript (indexing) expression, e.g. '[' expr ']'
			// We may have to backtrack if this turns out to be an array literal (e.g. "[1]a")

			skipExpectedToken(OPEN_SQUARE_BRACKET_TOKEN, p);

			ASTNodeRef index_expr = parseExpression(p);

			if(isTokenCurrent(CLOSE_SQUARE_BRACKET_TOKEN, p))
			{
				if(p.tokens[p.i].downcastToPtr<CLOSE_SQUARE_BRACKET_Token>()->suffix.size())
				{
					// This was a one-element collection literal, e.g "[1]a", then back-track.
					p.i = initial_pos;
					return left;
				}

				skipExpectedToken(CLOSE_SQUARE_BRACKET_TOKEN, p);

				// Could either return a FunctionExpression for 'elem', or an ArraySubscript ASTNode.
				left = new FunctionExpression(loc, "elem", left, index_expr);
			}
			else if(isTokenCurrent(COMMA_TOKEN, p)) // Then this was actually a collection literal after another expression, e.g. : " x < y [a, b, c]a" etc..
			{
				// Back-track
				p.i = initial_pos;
				return left;
			}
			else if(p.i >= p.tokens.size())
				throw LangParserExcep("End of buffer while parsing array subscript expression.");
			else
				throw LangParserExcep("Expected ']' or ','." + errorPosition(p));
		}
		else if(isTokenCurrent(DOT_TOKEN, p))
		{
			p.i++; // Skip DOT_TOKEN

			const std::string field_name = parseIdentifier("field name", p);

			left = new FunctionExpression(loc, field_name, left);
		}
		else
		{
			return left;
		}
	}
}


/*ASTNodeRef LangParser::parseParenExpression(ParseInfo& p)
{
	if(isTokenCurrent(OPEN_PARENTHESIS_TOKEN, p))
	{
		parseToken(OPEN_PARENTHESIS_TOKEN, p);

		ASTNodeRef e = parseExpression(p);

		parseToken(CLOSE_PARENTHESIS_TOKEN, p);

		return e;
	}
	else
	{
		return parseBasicExpression(p);
	}
}*/


ASTNodeRef LangParser::parseMapLiteralExpression(ParseInfo& p)
{
	SrcLocation loc = locationForParseInfo(p);

	parseToken(OPEN_BRACE_TOKEN, p);

	Reference<MapLiteral> m = new MapLiteral(loc);

	while(1)
	{
		if(isTokenCurrent(CLOSE_BRACE_TOKEN, p))
			break;

		// Parse key
		ASTNodeRef key = parseExpression(p);
		
		parseToken(COLON_TOKEN, p);

		// Parse value
		ASTNodeRef value = parseExpression(p);

		m->items.push_back(std::make_pair(key, value));

		if(isTokenCurrent(CLOSE_BRACE_TOKEN, p))
			break;

		parseToken(COMMA_TOKEN, p);
	}

	parseToken(CLOSE_BRACE_TOKEN, p);

	return m;
}


ASTNodeRef LangParser::parseArrayOrVectorOrTupleLiteral(ParseInfo& p)
{
	SrcLocation loc = locationForParseInfo(p);

	parseToken(OPEN_SQUARE_BRACKET_TOKEN, p);

	vector<ASTNodeRef> elems;

	while(1)
	{
		if(isTokenCurrent(CLOSE_SQUARE_BRACKET_TOKEN, p))
			break;

		// Parse element
		ASTNodeRef elem = parseExpression(p);
		
		elems.push_back(elem);

		if(isTokenCurrent(CLOSE_SQUARE_BRACKET_TOKEN, p))
			break;

		parseToken(COMMA_TOKEN, p);
	}

	if(!isTokenCurrent(CLOSE_SQUARE_BRACKET_TOKEN, p))
		throw LangParserExcep("Expected " + tokenName(CLOSE_SQUARE_BRACKET_TOKEN) + ", found " + tokenDescription(p.tokens[p.i]) + "." + errorPosition(p));

	const std::string& suffix = p.tokens[p.i].downcastToPtr<CLOSE_SQUARE_BRACKET_Token>()->suffix;

	skipExpectedToken(CLOSE_SQUARE_BRACKET_TOKEN, p);

	/*const bool is_subscript_operator = elems.size() == 1 && !isTokenCurrent(IDENTIFIER_TOKEN, p);

	if(is_subscript_operator)
	{
		FunctionExpressionRef func_expr = new FunctionExpression(loc);
		func_expr->function_name = "elem";
		func_expr->argument_expressions.push_back(main_expr);
		func_expr->argument_expressions.push_back(index_expr);
		return func_expr;
	}*/

	if(hasPrefix(suffix, "a"))
	{
		int int_suffix = 0;
		bool has_int_suffix = false;
		Parser temp_p(suffix.c_str(), (int)suffix.size());
		temp_p.advance(); // Advance past 'a'
		if(!temp_p.eof())
		{
			has_int_suffix = true;
			if(!temp_p.parseInt(int_suffix))
				throw LangParserExcep("Invalid square bracket literal suffix '" + suffix + "'.");
		}
		return ASTNodeRef(new ArrayLiteral(elems, loc, has_int_suffix, int_suffix));
	}
	else if(hasPrefix(suffix, "va"))
	{
		int int_suffix = 0;
		bool has_int_suffix = false;
		Parser temp_p(suffix.c_str(), (int)suffix.size());
		temp_p.advance(); // Advance past 'v'
		temp_p.advance(); // Advance past 'a'
		if(!temp_p.eof())
		{
			has_int_suffix = true;
			if(!temp_p.parseInt(int_suffix))
				throw LangParserExcep("Invalid square bracket literal suffix '" + suffix + "'.");
		}
		return new VArrayLiteral(elems, loc, has_int_suffix, int_suffix);
	}
	else if(hasPrefix(suffix, "v"))
	{
		int int_suffix = 0;
		bool has_int_suffix = false;
		Parser temp_p(suffix.c_str(), (int)suffix.size());
		temp_p.advance(); // Advance past 'v'
		if(!temp_p.eof())
		{
			has_int_suffix = true;
			if(!temp_p.parseInt(int_suffix))
				throw LangParserExcep("Invalid square bracket literal suffix '" + suffix + "'.");
		}
		return ASTNodeRef(new VectorLiteral(elems, loc, has_int_suffix, int_suffix));
	}
	if(hasPrefix(suffix, "t"))
	{
		return ASTNodeRef(new TupleLiteral(elems, loc));
	}
	else
	{
		//if(elems.size() > 1)
		//{
		//	// This is definitely a vector or array literal without the suffix.
			throw LangParserExcep("Unknown square bracket literal suffix '" + suffix + "'.");
		/*}
		else
		{
			// Treat this as an array subscript expression
			return new ArraySubscript(elems[0], loc);
		}*/
	}
}


/*ASTNodeRef LangParser::parseArraySubscriptExpression(ParseInfo& p)
{
	// Parse main expression
	ASTNodeRef main_expr = parseParenExpression(p); // parseBinaryLogicalExpression(p);

	SrcLocation loc = locationForParseInfo(p);

	const unsigned int initial_pos = p.i;

	if(isTokenCurrent(OPEN_SQUARE_BRACKET_TOKEN, p))
	{
		parseToken(OPEN_SQUARE_BRACKET_TOKEN, p);

		ASTNodeRef index_expr = parseExpression(p); // parseArraySubscriptExpression(p); // parseBinaryLogicalExpression(p);

		if(isTokenCurrent(CLOSE_SQUARE_BRACKET_TOKEN, p))
		{
			parseToken(CLOSE_SQUARE_BRACKET_TOKEN, p);

			if(isTokenCurrent(IDENTIFIER_TOKEN, p))
			{
				const std::string& id = p.tokens[p.i]->getIdentifierValue();
				if(id == "a" || id == "v" || id == "t")
				{
					// This was a one-element collection literal, e.g "[1]a"
					// Back-track
					p.i = initial_pos;
					return main_expr;
				}
			}


			// Could either return a FunctionExpression for 'elem', or a ArraySubscript ASTNode.

			FunctionExpressionRef func_expr = new FunctionExpression(loc);
			func_expr->function_name = "elem";
			func_expr->argument_expressions.push_back(main_expr);
			func_expr->argument_expressions.push_back(index_expr);
			return func_expr;
		}
		else if(isTokenCurrent(COMMA_TOKEN, p)) // Then this was actually a collection literal after another expression, e.g. : " x < y [a, b, c]a" etc..
		{
			// Back-track
			p.i = initial_pos;
			return main_expr;
		}
		else if(p.i >= p.tokens.size())
			throw LangParserExcep("End of buffer while parsing array subscript expression.");
		else
			throw LangParserExcep("Expected ']' or ','." + errorPosition(p));
	}

	return main_expr;
}*/

/*

[type] identifier ("," [type] identifier)* "="


*/
Reference<LetASTNode> LangParser::parseLet(ParseInfo& p)
{
	const SrcLocation loc = locationForParseInfo(p);

	vector<LetNodeVar> vars;

	while(1)
	{
		const unsigned int initial_pos = p.i;

		// Parse variable name or type
		std::string var_name = parseIdentifier("variable name", p);
		TypeRef declared_type;

		if(!(isTokenCurrent(EQUALS_TOKEN, p) || isTokenCurrent(COMMA_TOKEN, p)))
		{
			// Then assume what we parsed was the optional type.  So backtrack and re-parse.
			p.i = initial_pos; // backtrack
			declared_type = parseType(p);

			var_name = parseIdentifier("variable name", p);
		}

		LetNodeVar v;
		v.name = var_name;
		v.declared_type = declared_type;
		vars.push_back(v);

		if(isTokenCurrent(COMMA_TOKEN, p))
			p.i++; // Consume comma then loop
		else if(isTokenCurrent(EQUALS_TOKEN, p))
			break;
		else
			throw LangParserExcep("Expected ',' or '=' while parsing let." + errorPosition(p));
	}

	parseToken(EQUALS_TOKEN, p);

	ASTNodeRef expr = parseExpression(p);

	Reference<LetASTNode> letnode = new LetASTNode(vars, expr, loc);

	return letnode;
}


FunctionDefinitionRef LangParser::parseAnonFunction(ParseInfo& p)
{
	parseToken(BACK_SLASH_TOKEN, p);

	const std::string func_name = "anon_func_" + ::toString(p.i);

	FunctionDefinitionRef def = parseFunctionDefinitionGivenName(func_name, p, /*is_lambda=*/true);

	//def->use_captured_vars = true;
	def->is_anon_func = true;

	// Add this anon function to list of parsed function definitions.
	//p.top_level_defs.push_back(def);
	return def;

	/*
	// Parse parameter list
	vector<FunctionDefinition::FunctionArg> args;
	parseParameterList(p, vector<string>(), args);

	// Parse return type
	TypeRef return_type = parseType(p, vector<string>());

	parseToken(COLON_TOKEN, p);

	// Parse function body
	//parseToken(OPEN_PARENTHESIS_TOKEN, p);

	ASTNodeRef body_expr = parseExpression(p);

	//parseToken(CLOSE_PARENTHESIS_TOKEN, p);

	FunctionDefinition* func = new FunctionDefinition(
		"anon",
		args,
		vector<Reference<LetASTNode> >(),
		body_expr,
		return_type,
		false // constructor
	);
	//AnonFunction* func = new AnonFunction();
	//func->args = args;
	//func->body = body_expr;


	
	//vector<TypeRef> argtypes;
	//for(unsigned int i=0; i<args.size(); ++i)
	//	argtypes.push_back(args[i].type);

	//func->thetype = TypeRef(new Function(
	//	argtypes,
	//	return_type
	//));
	//

	return ASTNodeRef(func);*/
}


void LangParser::parseParameterList(ParseInfo& p, std::vector<FunctionDefinition::FunctionArg>& args_out)
{
	parseToken(OPEN_PARENTHESIS_TOKEN, p);

	while(1)
	{
		if(p.i >= p.tokens.size())
			throw LangParserExcep("End of buffer before end of parameter list.");
		else if(p.tokens[p.i]->getType() == CLOSE_PARENTHESIS_TOKEN)
		{
			p.i++;
			break;
		}

		TypeVRef param_type = parseType(p);
		const std::string param_name = parseIdentifier("parameter name", p);

		args_out.push_back(FunctionDefinition::FunctionArg(param_type, param_name));

		if(p.i >= p.tokens.size())
		{
			throw LangParserExcep("End of buffer before end of parameter list.");
		}
		else if(p.tokens[p.i]->getType() == CLOSE_PARENTHESIS_TOKEN)
		{
			p.i++;
			break;
		}
		else if(p.tokens[p.i]->getType() == COMMA_TOKEN)
		{
			p.i++;
		}
		else
		{
			throw LangParserExcep("Expected ',' or ')' while parsing parameter list of function. " + errorPosition(p));
		}
	}

}


const std::string LangParser::errorPosition(const SourceBuffer& buffer, size_t char_index)
{
	return Diagnostics::positionString(buffer, char_index);
}


const std::string LangParser::errorPosition(const ParseInfo& p)
{
	if(p.i < (unsigned int)p.tokens.size())
		return errorPosition(*p.text_buffer, p.tokens[p.i]->char_index);
	else
	{
		// End of buffer.
		if(p.tokens.empty())
			return "end of buffer";
		else
			return errorPosition(*p.text_buffer, (unsigned int)p.text_buffer->source.size() - 1);
	}
}


const std::string LangParser::errorPositionPrevToken(ParseInfo& p)
{
	if(p.i >= 1 && p.i < p.tokens.size() + 1)
		return errorPosition(*p.text_buffer, p.tokens[p.i - 1]->char_index);
	else
		return "Unknown";
}


#if BUILD_TESTS


void LangParser::test()
{
	const std::string s = "def lerp(real a, real b, real t) real : add(mul(a, sub(1.0, t)), mul(b, t))";
	SourceBufferRef buffer(new SourceBuffer("buffer", s));
	//const std::string s = "def lerp(real a, real b, real t) real : add(a, b, t())";

	std::vector<Reference<TokenBase> > tokens;
	Lexer::process(buffer, tokens);
	try
	{
		LangParser lp(true, true);
//		Reference<ASTNode> root = lp.parseBuffer(tokens, s.c_str());
	
//		testAssert(root->nodeType() == ASTNode::BufferRootType);
		
		//Reference<ASTNode> lerp = dynamic_cast<BufferRoot*>(root.getPointer())->children[0];
		//testAssert(lerp->getType() == ASTNode::FunctionDefinitionType);

/*		Reference<ASTNode> add = lerp->children[0];
		testAssert(add->type == ASTNode::FUNCTION_EXPRESSION);

		Reference<ASTNode> lmul = add->children[0];
		testAssert(lmul->type == ASTNode::FUNCTION_EXPRESSION);
		
		Reference<ASTNode> a = lmul->children[0];
		testAssert(a->type == ASTNode::VARIABLE);

		Reference<ASTNode> sub = lmul->children[1];
		testAssert(sub->type == ASTNode::FUNCTION_EXPRESSION);

		Reference<ASTNode> one = sub->children[0];
		testAssert(one->type == ASTNode::REAL_LITERAL);

		Reference<ASTNode> lt = sub->children[1];
		testAssert(lt->type == ASTNode::VARIABLE);

		Reference<ASTNode> rmul = add->children[1];
		testAssert(rmul->type == ASTNode::FUNCTION_EXPRESSION);

		Reference<ASTNode> b = rmul->children[0];
		testAssert(b->type == ASTNode::VARIABLE);

		Reference<ASTNode> rt = rmul->children[1];
		testAssert(rt->type == ASTNode::VARIABLE);
*/
	}
	catch(LangParserExcep& e)
	{
		conPrint("LangParserExcep: " + e.what());
		testAssert(false);
	}
}


#endif // BUILD_TESTS


} // end namespace Winter
