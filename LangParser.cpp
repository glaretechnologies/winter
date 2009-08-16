/*=====================================================================
LangParser.cpp
--------------
File created by ClassTemplate on Wed Jun 11 02:56:20 2008
Code By Nicholas Chapman.

Copyright 2009 Nicholas Chapman
=====================================================================*/
#include "LangParser.h"


#include "Lexer.h"
#include "ASTNode.h"
#include "../../indigosvn/trunk/indigo/TestUtils.h"
#include "../../indigosvn/trunk/indigo/globals.h"
#include "../../indigosvn/trunk/utils/stringutils.h"
#include <assert.h>
#include <map>


namespace Winter
{


LangParser::LangParser()
{
}


LangParser::~LangParser()
{
}


Reference<ASTNode> LangParser::parseBuffer(const std::vector<Reference<TokenBase> >& tokens, const char* text_buffer)
{
	try
	{
		BufferRoot* root = new BufferRoot();
		//Reference<ASTNode> root( new BufferRoot() );

		unsigned int i = 0;

		ParseInfo parseinfo(i, tokens);
		parseinfo.text_buffer = text_buffer;

		while(i < tokens.size())
		{
			if(tokens[i]->isIdentifier() && tokens[i]->getIdentifierValue() == "def")
				root->func_defs.push_back(parseFunctionDefinition(root, parseinfo));
			//else if(tokens[i]->isIdentifier() && tokens[i]->getIdentifierValue() == "declare")
			//	root->children.push_back(parseFunctionDeclaration(tokens, text_buffer, i));
			else
				throw LangParserExcep("Expected 'def'." + errorPosition(text_buffer, tokens[i]->char_index));
		}

		return Reference<ASTNode>(root);
	}
	catch(TokenBaseExcep& e)
	{
		throw LangParserExcep("TokenBaseExcep: " + e.what());
	}
}


const std::string LangParser::parseIdentifier(const std::string& id_type, const ParseInfo& p)
{
	if(p.i >= p.tokens.size())
		throw LangParserExcep("End of buffer before " + id_type + " identifier.");

	if(!p.tokens[p.i]->isIdentifier())
		throw LangParserExcep("Expected " + id_type + " identifier." + errorPosition(p.text_buffer, p.tokens[p.i]->char_index));

	return p.tokens[p.i++]->getIdentifierValue();
}


void LangParser::parseToken(unsigned int token_type, const ParseInfo& p)
{
	if(p.i >= p.tokens.size())
		throw LangParserExcep("End of buffer before " + tokenName(token_type) + " token.");
	
	if(p.tokens[p.i]->getType() != token_type)
		throw LangParserExcep("Expected " + tokenName(token_type) + "." + errorPosition(p.text_buffer, p.tokens[p.i]->char_index));

	p.i++;
}


bool LangParser::isTokenCurrent(unsigned int token_type, const ParseInfo& p)
{
	return p.i < p.tokens.size() && p.tokens[p.i]->getType() == token_type;
}


ASTNodeRef LangParser::parseVariableExpression(ASTNode* parent, const ParseInfo& p)
{
	const std::string name = parseIdentifier("variable name", p);
	Variable* var = new Variable(parent, name);
	return ASTNodeRef(var);
}


Reference<FunctionDefinition> LangParser::parseFunctionDefinition(ASTNode* parent, const ParseInfo& p)
{
	//Reference<ASTNode> node( new ASTNode(ASTNode::FUNCTION_DEFINITION) );
	//FunctionDefinition* node = new FunctionDefinition();

	parseIdentifier("def", p);

	const std::string function_name = parseIdentifier("function name", p);

	// Parse parameter list
	std::vector<FunctionDefinition::FunctionArg> args;
	parseParameterList(p, args);
	
	//parseToken(tokens, text_buffer, Token::RIGHT_ARROW, i);

	// Parse return type
	TypeRef return_type = parseType(p);

	parseToken(COLON_TOKEN, p);
	
	Reference<FunctionDefinition> def(new FunctionDefinition(
		//parent,
		function_name,
		args,
		//body,
		return_type
		));

	// Parse any 'lets'
	if(p.i < p.tokens.size() && p.tokens[p.i]->isIdentifier() && p.tokens[p.i]->getIdentifierValue() == "let")
	{
		Reference<LetASTNode> let = parseLet(def.getPointer(), p);
		def->lets.push_back(let);
	}

	// Parse function body
	ASTNodeRef body = parseExpression(def.getPointer(), p);

	def->body = body;
	return def;
}


Reference<ASTNode> LangParser::parseFunctionExpression(ASTNode* parent, const ParseInfo& p)
{
	const std::string func_name = parseIdentifier("function name", p);

	// Parse parameter list
	parseToken(OPEN_PARENTHESIS_TOKEN, p);

	if(p.i == p.tokens.size())
		throw LangParserExcep("Expected ')'");

	std::vector<Reference<ASTNode> > arg_expressions;

	FunctionExpression* expr = new FunctionExpression(/*parent*/);

	if(p.tokens[p.i]->getType() != CLOSE_PARENTHESIS_TOKEN)
	{
		arg_expressions.push_back(parseExpression(expr, p));
	}

	if(p.tokens[p.i]->getType() != CLOSE_PARENTHESIS_TOKEN)
	{
		parseToken(COMMA_TOKEN, p);
	
		arg_expressions.push_back(parseExpression(expr, p));
	}

	parseToken(CLOSE_PARENTHESIS_TOKEN, p);

	expr->argument_expressions = arg_expressions;
	expr->function_name = func_name;
	return ASTNodeRef(expr);
}


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


ASTNodeRef LangParser::parseLiteral(const ParseInfo& p)
{
	if(p.tokens[p.i]->getType() == INT_LITERAL_TOKEN)
	{
		return Reference<ASTNode>( new IntLiteral(p.tokens[p.i++]->getIntLiteralValue()) );
	}
	else if(p.tokens[p.i]->getType() == FLOAT_LITERAL_TOKEN)
	{
		return Reference<ASTNode>( new FloatLiteral(p.tokens[p.i++]->getFloatLiteralValue()) );
	}
	else if(p.tokens[p.i]->getType() == STRING_LITERAL_TOKEN)
	{
		return Reference<ASTNode>( new StringLiteral(p.tokens[p.i++]->getStringLiteralValue()) );
	}
	else if(p.tokens[p.i]->getType() == BOOL_LITERAL_TOKEN)
	{
		return Reference<ASTNode>( new BoolLiteral(p.tokens[p.i++]->getBoolLiteralValue()) );
	}
	else
	{
		throw LangParserExcep("token is not a literal");
	}
}


ASTNodeRef LangParser::parseExpression(ASTNode* parent, const ParseInfo& p)
{
	/*if(tokens[i]->isLiteral())
	{
		ASTNodeRef n = ASTNodeForLiteral(parent, tokens[i]);
		i++;
		return n;
	}
	else if(tokens[i]->isIdentifier())
	{
		// If next token is a '(', then this is a function expression
		if(i + 1 < tokens.size() && tokens[i+1]->getType() == OPEN_PARENTHESIS_TOKEN)
			return parseFunctionExpression(parent, tokens, text_buffer, i);
		else if(i + 1 < tokens.size() && tokens[i+1]->getType() == PLUS_TOKEN)
			return parseAdditionExpression(parent, tokens, text_buffer, i);
		else
			return parseVariableExpression(parent, tokens, text_buffer, i);
	}
	else
	{
		throw LangParserExcep("Expected literal or identifier in expression.");
	}*/
	return parseAddSubExpression(parent, p);
}


ASTNodeRef LangParser::parseBasicExpression(ASTNode* parent, const ParseInfo& p)
{
	if(p.tokens[p.i]->isLiteral())
	{
		return parseLiteral(p);
	}
	else if(p.tokens[p.i]->isIdentifier())
	{
		// If next token is a '(', then this is a function expression
		if(p.i + 1 < p.tokens.size() && p.tokens[p.i+1]->getType() == OPEN_PARENTHESIS_TOKEN)
			return parseFunctionExpression(parent, p);
		else
			return parseVariableExpression(parent, p);
	}
	else if(p.tokens[p.i]->getType() == OPEN_BRACE_TOKEN)
	{
		return parseMapLiteralExpression(parent, p);
	}
	else if(p.tokens[p.i]->getType() == BACK_SLASH_TOKEN)
	{
		return parseAnonFunction(parent, p);
	}
	else
	{
		throw LangParserExcep("Expected literal or identifier in expression.");
	}
}


TypeRef LangParser::parseType(const ParseInfo& p)
{
	const std::string t = parseIdentifier("type", p);
	if(t == "float")
		return TypeRef(new Float());
	else if(t == "int")
		return TypeRef(new Int());
	else if(t == "string")
		return TypeRef(new String());
	else if(t == "map")
		return parseMapType(p);
	else if(t == "function")
		return parseFunctionType(p);
	else
		throw LangParserExcep("Unknown type '" + t + "'.");
}


TypeRef LangParser::parseMapType(const ParseInfo& p)
{
	parseToken(LEFT_ANGLE_BRACKET_TOKEN, p);

	TypeRef from = parseType(p);

	parseToken(COMMA_TOKEN, p);

	TypeRef to = parseType(p);

	parseToken(RIGHT_ANGLE_BRACKET_TOKEN, p);

	return TypeRef(new Map(from, to));
}


ASTNodeRef LangParser::parseAddSubExpression(ASTNode* parent, const ParseInfo& p)
{
	ASTNodeRef left = parseMulDivExpression(parent, p);
	if(isTokenCurrent(PLUS_TOKEN, p))
	{
		parseToken(PLUS_TOKEN, p);

		AdditionExpression* addexpr = new AdditionExpression();
		addexpr->a = left;
		//left->setParent(addexpr);
		addexpr->b = parseMulDivExpression(parent, p);
		return ASTNodeRef(addexpr);
	}
	else if(isTokenCurrent(MINUS_TOKEN, p))
	{
		parseToken(MINUS_TOKEN, p);

		SubtractionExpression* e = new SubtractionExpression();
		e->a = left;
		//left->setParent(e);
		e->b = parseMulDivExpression(parent, p);
		return ASTNodeRef(e);
	}
	else
		return left;
}


ASTNodeRef LangParser::parseMulDivExpression(ASTNode* parent, const ParseInfo& p)
{
	ASTNodeRef left = parseParenExpression(parent, p);
	if(isTokenCurrent(ASTERISK_TOKEN, p))
	{
		parseToken(ASTERISK_TOKEN, p);

		MulExpression* addexpr = new MulExpression();
		addexpr->a = left;
		//left->setParent(addexpr);
		addexpr->b = parseParenExpression(parent, p);
		return ASTNodeRef(addexpr);
	}
	else
		return left;
}


ASTNodeRef LangParser::parseParenExpression(ASTNode* parent, const ParseInfo& p)
{
	if(isTokenCurrent(OPEN_PARENTHESIS_TOKEN, p))
	{
		parseToken(OPEN_PARENTHESIS_TOKEN, p);

		ASTNodeRef e = parseExpression(parent, p);

		parseToken(CLOSE_PARENTHESIS_TOKEN, p);

		return e;
	}
	else
	{
		return parseBasicExpression(parent, p);
	}
}


ASTNodeRef LangParser::parseMapLiteralExpression(ASTNode* parent, const ParseInfo& p)
{
	parseToken(OPEN_BRACE_TOKEN, p);

	MapLiteral* m = new MapLiteral();

	while(1)
	{
		if(isTokenCurrent(CLOSE_BRACE_TOKEN, p)) // if(i < tokens.size() && tokens[i]->getType() == CLOSE_BRACE_TOKEN)
			break;

		// Parse key
		ASTNodeRef key = parseExpression(m, p);
		
		parseToken(COLON_TOKEN, p);

		// Parse value
		ASTNodeRef value = parseExpression(m, p);

		m->items.push_back(std::make_pair(key, value));

		if(isTokenCurrent(CLOSE_BRACE_TOKEN, p))//if(p.i < .itokens.size() && tokens[i]->getType() == CLOSE_BRACE_TOKEN)
			break;

		parseToken(COMMA_TOKEN, p);
	}

	parseToken(CLOSE_BRACE_TOKEN, p);

	return ASTNodeRef(m);
}


Reference<LetASTNode> LangParser::parseLet(ASTNode* parent, const ParseInfo& p)
{
	parseIdentifier("let", p);

	const std::string var_name = parseIdentifier("variable name", p);

	parseToken(EQUALS_TOKEN, p);

	Reference<LetASTNode> letnode = Reference<LetASTNode>(new LetASTNode(var_name));

	ASTNodeRef expr = parseExpression(letnode.getPointer(), p);

	letnode->expr = expr;

	return letnode;
}


ASTNodeRef LangParser::parseAnonFunction(ASTNode* parent, const ParseInfo& p)
{
	parseToken(BACK_SLASH_TOKEN, p);

	// Parse parameter list
	std::vector<FunctionDefinition::FunctionArg> args;
	parseParameterList(p, args);

	// Parse return type
	TypeRef return_type = parseType(p);

	parseToken(COLON_TOKEN, p);

	// Parse function body
	//parseToken(OPEN_PARENTHESIS_TOKEN, p);

	ASTNodeRef body_expr = parseExpression(parent, p);

	//parseToken(CLOSE_PARENTHESIS_TOKEN, p);

	AnonFunction* func = new AnonFunction();
	func->args = args;
	func->body = body_expr;


	vector<TypeRef> argtypes;
	for(unsigned int i=0; i<args.size(); ++i)
		argtypes.push_back(args[i].type);

	func->thetype = TypeRef(new Function(
		argtypes,
		return_type
	));

	return ASTNodeRef(func);
}


void LangParser::parseParameterList(const ParseInfo& p, std::vector<FunctionDefinition::FunctionArg>& args_out)
{
	args_out.resize(0);

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

		TypeRef param_type = parseType(p);
		const std::string param_name = parseIdentifier("parameter name", p);

		args_out.push_back(FunctionDefinition::FunctionArg());
		args_out.back().name = param_name;
		args_out.back().type = param_type;

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
			throw LangParserExcep("Expected ',' or ')' while parsing parameter list of function. " + errorPosition(p.text_buffer, p.tokens[p.i]->char_index));
		}
	}

}


const std::string LangParser::errorPosition(const std::string& buffer, unsigned int pos)
{
	unsigned int line, col;
	StringUtils::getPosition(buffer, pos, line, col);
	return "  Line " + toString(line + 1) + ", column " + toString(col + 1);
}


void LangParser::test()
{
	const std::string s = "def lerp(real a, real b, real t) real : add(mul(a, sub(1.0, t)), mul(b, t))";

	//const std::string s = "def lerp(real a, real b, real t) real : add(a, b, t())";

	std::vector<Reference<TokenBase> > tokens;
	Lexer::process(s, tokens);
	try
	{
		LangParser lp;
		Reference<ASTNode> root = lp.parseBuffer(tokens, s.c_str());
	
		testAssert(root->nodeType() == ASTNode::BufferRootType);
		
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


} //end namespace Winter
