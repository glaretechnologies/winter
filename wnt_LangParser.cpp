/*=====================================================================
LangParser.cpp
--------------
File created by ClassTemplate on Wed Jun 11 02:56:20 2008
Code By Nicholas Chapman.

Copyright 2009 Nicholas Chapman
=====================================================================*/
#include "wnt_LangParser.h"


#include "wnt_Lexer.h"
#include "wnt_ASTNode.h"
#include "BuiltInFunctionImpl.h"
#include "indigo/TestUtils.h"
#include "indigo/globals.h"
#include "utils/stringutils.h"
#include <assert.h>
#include <map>
#include "maths/mathstypes.h"


namespace Winter
{


LangParser::LangParser()
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


Reference<ASTNode> LangParser::parseBuffer(const std::vector<Reference<TokenBase> >& tokens, const char* text_buffer,
										   vector<FunctionDefinitionRef>& func_defs_out)
{
	try
	{
		BufferRoot* root = new BufferRoot();

		

		//TEMP:
		// Create float array map definition
		/*{
			vector<FunctionDefinition::FunctionArg> args(2);
			args[0].type = TypeRef(new Function(vector<TypeRef>(1, TypeRef(new Float())), TypeRef(new Float())));
			args[0].name = "f";
			args[1].type = TypeRef(new ArrayType(TypeRef(new Float)));
			args[1].name = "array";

			FunctionDefinition* def = new FunctionDefinition(
				"map",
				args,
				vector<Reference<LetASTNode> >(),
				ASTNodeRef(NULL), // body expr
				TypeRef(new ArrayType(TypeRef(new Float))), // return type
				new ArrayMapBuiltInFunc(
					TypeRef(new ArrayType(TypeRef(new Float))),
					Reference<Function>(new Function(vector<TypeRef>(1, TypeRef(new Float())), TypeRef(new Float())))
				)
			);

			root->func_defs.push_back(Reference<FunctionDefinition>(def));
		}*/

		// TEMP: Create float array fold definition
		// #decl fold<T>(function<T, T, T>, array<T>, T) T
		{
			vector<FunctionDefinition::FunctionArg> args(3);
			TypeRef T(new GenericType(0));
			args[0].type = TypeRef(new Function(
				vector<TypeRef>(2, T), // arg types
				T, // return type
				vector<TypeRef>(), // captured var types
				false // use_captured_vars
			));
			args[0].name = "f";
			args[1].type = TypeRef(new ArrayType(T));
			args[1].name = "array";
			args[2].type = T;
			args[2].name = "initial_val";

			FunctionDefinition* def = new FunctionDefinition(
				"fold",
				args,
				//vector<Reference<LetASTNode> >(),
				ASTNodeRef(NULL), // body expr
				T, // return type
				//new ArrayFoldBuiltInFunc(
				//	TypeRef(new Float)
				//)
				NULL // built in impl.
			);

			root->func_defs.push_back(Reference<FunctionDefinition>(def));
		}

		// Create 'if' built in function
		{
			vector<FunctionDefinition::FunctionArg> args(3);
			args[0].name = "condition";
			args[0].type = TypeRef(new Bool());

			TypeRef T(new GenericType(
				0 // generic_type_param_index
			));

			args[1].type = T;
			args[1].name = "a";
			args[2].type = T;
			args[2].name = "b";

			FunctionDefinition* def = new FunctionDefinition(
				"if", // name
				args, // args
				//vector<Reference<LetASTNode> >(), // lets
				ASTNodeRef(NULL), // body expr
				T, // return type
				new IfBuiltInFunc(T) // built in impl.
			);

			root->func_defs.push_back(Reference<FunctionDefinition>(def));
		}

		// Create 'dot' (dot product) built-in function
		{
			TypeRef vec4f_type(new VectorType(TypeRef(new Float()), 4));
			Reference<VectorType> vec4f_type_v(new VectorType(TypeRef(new Float()), 4));

			vector<FunctionDefinition::FunctionArg> args(2);
			args[0].name = "a";
			args[0].type = vec4f_type;
			args[1].name = "b";
			args[1].type = vec4f_type;

			FunctionDefinition* def = new FunctionDefinition(
				"dot", // name
				args, // args
				//vector<Reference<LetASTNode> >(), // lets
				ASTNodeRef(NULL), // body expr
				TypeRef(new Float()), // return type
				new DotProductBuiltInFunc(vec4f_type_v) // built in impl.
				);

			root->func_defs.push_back(Reference<FunctionDefinition>(def));
		}

		// Create vec4f min built-in function
		{
			TypeRef vec4f_type(new VectorType(TypeRef(new Float()), 4));
			Reference<VectorType> vec4f_type_v(new VectorType(TypeRef(new Float()), 4));

			vector<FunctionDefinition::FunctionArg> args(2);
			args[0].name = "a";
			args[0].type = vec4f_type;
			args[1].name = "b";
			args[1].type = vec4f_type;

			FunctionDefinition* def = new FunctionDefinition(
				"min", // name
				args, // args
				//vector<Reference<LetASTNode> >(), // lets
				ASTNodeRef(NULL), // body expr
				vec4f_type, // return type
				new VectorMinBuiltInFunc(vec4f_type_v) // built in impl.
				);

			root->func_defs.push_back(Reference<FunctionDefinition>(def));
		}

		// Create vec4f max built-in function
		{
			TypeRef vec4f_type(new VectorType(TypeRef(new Float()), 4));
			Reference<VectorType> vec4f_type_v(new VectorType(TypeRef(new Float()), 4));

			vector<FunctionDefinition::FunctionArg> args(2);
			args[0].name = "a";
			args[0].type = vec4f_type;
			args[1].name = "b";
			args[1].type = vec4f_type;

			FunctionDefinition* def = new FunctionDefinition(
				"max", // name
				args, // args
				//vector<Reference<LetASTNode> >(), // lets
				ASTNodeRef(NULL), // body expr
				vec4f_type, // return type
				new VectorMaxBuiltInFunc(vec4f_type_v) // built in impl.
				);

			root->func_defs.push_back(Reference<FunctionDefinition>(def));
		}

		// Create 'pow' built-in function
		{
			vector<FunctionDefinition::FunctionArg> args(2);
			args[0].name = "a";
			args[0].type = TypeRef(new Float());
			args[1].name = "b";
			args[1].type = TypeRef(new Float());

			FunctionDefinition* def = new FunctionDefinition(
				"pow", // name
				args, // args
				//vector<Reference<LetASTNode> >(), // lets
				ASTNodeRef(NULL), // body expr
				TypeRef(new Float()), // return type
				new PowBuiltInFunc() // built in impl.
				);

			root->func_defs.push_back(Reference<FunctionDefinition>(def));
		}

		// Create 'sqrt' built-in function
		{
			vector<FunctionDefinition::FunctionArg> args(1);
			args[0].name = "x";
			args[0].type = TypeRef(new Float());

			FunctionDefinition* def = new FunctionDefinition(
				"sqrt", // name
				args, // args
				//vector<Reference<LetASTNode> >(), // lets
				ASTNodeRef(NULL), // body expr
				TypeRef(new Float()), // return type
				new SqrtBuiltInFunc() // built in impl.
				);

			root->func_defs.push_back(Reference<FunctionDefinition>(def));
		}


		// Create 'sin' built-in function
		{
			vector<FunctionDefinition::FunctionArg> args(1);
			args[0].name = "x";
			args[0].type = TypeRef(new Float());

			FunctionDefinition* def = new FunctionDefinition(
				"sin", // name
				args, // args
				//vector<Reference<LetASTNode> >(), // lets
				ASTNodeRef(NULL), // body expr
				TypeRef(new Float()), // return type
				new SinBuiltInFunc() // built in impl.
				);

			root->func_defs.push_back(Reference<FunctionDefinition>(def));
		}


		// Create 'cos' built-in function
		{
			vector<FunctionDefinition::FunctionArg> args(1);
			args[0].name = "x";
			args[0].type = TypeRef(new Float());

			FunctionDefinition* def = new FunctionDefinition(
				"cos", // name
				args, // args
				//vector<Reference<LetASTNode> >(), // lets
				ASTNodeRef(NULL), // body expr
				TypeRef(new Float()), // return type
				new CosBuiltInFunc() // built in impl.
				);

			root->func_defs.push_back(Reference<FunctionDefinition>(def));
		}

		func_defs_out = root->func_defs;



		std::map<std::string, TypeRef> named_types;

		//Reference<ASTNode> root( new BufferRoot() );

		unsigned int i = 0;

		ParseInfo parseinfo(i, tokens, named_types, func_defs_out);
		parseinfo.text_buffer = text_buffer;

		while(i < tokens.size())
		{
			if(tokens[i]->isIdentifier() && tokens[i]->getIdentifierValue() == "def")
				root->func_defs.push_back(parseFunctionDefinition(parseinfo));
			else if(tokens[i]->isIdentifier() && tokens[i]->getIdentifierValue() == "struct")
			{
				Reference<StructureType> t = parseStructType(parseinfo, vector<string>());
				// TODO: check to see if it has already been defined.
				named_types[t->name] = TypeRef(t.getPointer()); 

				// Make constructor function for this structure
				vector<FunctionDefinition::FunctionArg> args(t->component_types.size());
				for(unsigned int i=0; i<args.size(); ++i)
				{
					args[i].name = t->component_names[i];
					args[i].type = t->component_types[i];
				}

				FunctionDefinition* cons = new FunctionDefinition(
					t->name, // name
					args, // arguments
					//vector<Reference<LetASTNode> >(), // lets
					ASTNodeRef(NULL), // body expr
					TypeRef(t.getPointer()), // declard return type
					new Constructor(t) // built in func impl.
				);
				root->func_defs.push_back(FunctionDefinitionRef(cons));
				func_defs_out.push_back(FunctionDefinitionRef(cons));

				// Make field access functions
				vector<FunctionDefinition::FunctionArg> getfield_args(1);
				getfield_args[0].name = "s";
				getfield_args[0].type = TypeRef(t.getPointer());

				for(unsigned int i=0; i<t->component_types.size(); ++i)
				{
					Reference<FunctionDefinition> def(new FunctionDefinition(
						t->component_names[i], // name
						getfield_args, // args
						//vector<Reference<LetASTNode> >(), // lets
						ASTNodeRef(NULL), // body expr
						t->component_types[i], // return type
						new GetField(t, i) // impl
					));

					root->func_defs.push_back(def);
					func_defs_out.push_back(def);
				}
			}
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
	{
		throw LangParserExcep("Expected " + tokenName(token_type) + ", found " + tokenName(p.tokens[p.i]->getType()) + errorPosition(p.text_buffer, p.tokens[p.i]->char_index));
	}
	p.i++;
}


bool LangParser::isTokenCurrent(unsigned int token_type, const ParseInfo& p)
{
	return p.i < p.tokens.size() && p.tokens[p.i]->getType() == token_type;
}


ASTNodeRef LangParser::parseFieldExpression(const ParseInfo& p)
{
//	ASTNodeRef var_expression = parseVariableExpression(p);
	ASTNodeRef var_expression;

	// If next token is a '(', then this is a function expression
	if(p.i + 1 < p.tokens.size() && p.tokens[p.i+1]->getType() == OPEN_PARENTHESIS_TOKEN)
		var_expression = parseFunctionExpression(p);
	else
		var_expression = parseVariableExpression(p);


	/*if(isTokenCurrent(DOT_TOKEN, p))
	{
		parseToken(DOT_TOKEN, p);

		const std::string field_name = parseIdentifier("field name", p);

		FunctionExpression* func_expr(new FunctionExpression());
		func_expr->function_name = field_name;
		func_expr->argument_expressions.push_back(var_expression);
		return ASTNodeRef(func_expr);
	}*/
	while(isTokenCurrent(DOT_TOKEN, p))
	{
		parseToken(DOT_TOKEN, p);

		const std::string field_name = parseIdentifier("field name", p);

		FunctionExpression* func_expr(new FunctionExpression());
		func_expr->function_name = field_name;
		func_expr->argument_expressions.push_back(var_expression);
		var_expression = ASTNodeRef(func_expr);
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


ASTNodeRef LangParser::parseVariableExpression(const ParseInfo& p)
{
	const std::string name = parseIdentifier("variable name", p);
	if(isKeyword(name))
		throw LangParserExcep("Cannot call a variable '" + name + "' - is a keyword.  " +  errorPositionPrevToken(p));

	Variable* var = new Variable(name);
	return ASTNodeRef(var);
}


bool LangParser::isKeyword(const std::string& name)
{
	return 
		name == "let" ||
		name == "def" ||
		name == "in";
	// TODO: finish
}



Reference<FunctionDefinition> LangParser::parseFunctionDefinition(const ParseInfo& p)
{
	parseIdentifier("def", p);

	const std::string function_name = parseIdentifier("function name", p);

	Reference<FunctionDefinition> def = parseFunctionDefinitionGivenName(function_name, p);

	// Add this function def to the list of parsed function definitions.
	p.func_defs.push_back(def);

	return def;
}


FunctionDefinitionRef LangParser::parseFunctionDefinitionGivenName(const std::string& func_name, const ParseInfo& p)
{
	try
	{
		// Parse generic parameters, if present
		vector<string> generic_type_params;
		if(isTokenCurrent(LEFT_ANGLE_BRACKET_TOKEN, p))
		{
			parseToken(LEFT_ANGLE_BRACKET_TOKEN, p);

			generic_type_params.push_back(parseIdentifier("type parameter", p));

			while(isTokenCurrent(COMMA_TOKEN, p))
			{
				parseToken(COMMA_TOKEN, p);
				generic_type_params.push_back(parseIdentifier("type parameter", p));
			}

			parseToken(RIGHT_ANGLE_BRACKET_TOKEN, p);
		}


		// Parse parameter list
		std::vector<FunctionDefinition::FunctionArg> args;
		parseParameterList(p, generic_type_params, args);

		// Fill in generic_type_param_index for all generic types
		//for(unsigned int i=0; i<args.size(); ++i)
		//	for(unsigned int z=0; z<generic_type_params.size(); ++z)
		//		if(generic_type_params[z] == args[i].
		
		//parseToken(tokens, text_buffer, Token::RIGHT_ARROW, i);

		// Parse optional return type
		TypeRef return_type(NULL);
		if(!isTokenCurrent(COLON_TOKEN, p))
		{
			return_type = parseType(p, generic_type_params);
		}
		
		parseToken(COLON_TOKEN, p);
		
		
		/*vector<Reference<LetASTNode> > lets;

		// Parse any 'lets'
		while(p.i < p.tokens.size() && p.tokens[p.i]->isIdentifier() && p.tokens[p.i]->getIdentifierValue() == "let")
		{
			Reference<LetASTNode> let = parseLet(p);
			lets.push_back(let);
		}*/

		// Parse function body
		ASTNodeRef body = parseLetBlock(p); //parseExpression(p);

		Reference<FunctionDefinition> def(new FunctionDefinition(
			func_name,
			args,
			//lets,
			body,
			return_type, // declared return type
			NULL // built in func impl
			));

		return def;
	}
	catch(LangParserExcep& e)
	{
		throw LangParserExcep("Error occurred while parsing function '" + func_name + "': " + e.what());
	}
}


Reference<ASTNode> LangParser::parseFunctionExpression(const ParseInfo& p)
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


Reference<IntLiteral> LangParser::parseIntLiteral(const ParseInfo& p)
{
	if(p.tokens[p.i]->getType() == INT_LITERAL_TOKEN)
	{
		return Reference<IntLiteral>( new IntLiteral(p.tokens[p.i++]->getIntLiteralValue()) );
	}
	else
	{
		throw LangParserExcep("token is not an integer literal");
	}
}


Reference<ASTNode> LangParser::parseLetBlock(const ParseInfo& p)
{
	if(p.i < p.tokens.size() && p.tokens[p.i]->isIdentifier() && p.tokens[p.i]->getIdentifierValue() == "let")
	{
		p.i++;

		vector<Reference<LetASTNode> > lets;
		
		while(p.i < p.tokens.size() && !(p.tokens[p.i]->isIdentifier() && p.tokens[p.i]->getIdentifierValue() == "in"))
		{
			Reference<LetASTNode> let = parseLet(p);
			lets.push_back(let);
		}

		//TODO: better error msg
		const std::string in = parseIdentifier("in", p);
		if(in != "in")
			throw BaseException("Missing 'in' after let expressions.");

		ASTNodeRef main_expr = parseLetBlock(p);

		return ASTNodeRef(new LetBlock(main_expr, lets));
	}


	return parseExpression(p);
}


ASTNodeRef LangParser::parseExpression(const ParseInfo& p)
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
	return parseAddSubExpression(p);
}


ASTNodeRef LangParser::parseBasicExpression(const ParseInfo& p)
{
	if(p.i >= p.tokens.size())
		throw LangParserExcep("End of buffer while parsing basic expression.");

	if(p.tokens[p.i]->isLiteral())
	{
		return parseLiteral(p);
	}
	else if(p.tokens[p.i]->isIdentifier())
	{
		return parseFieldExpression(p);

		/*
		// If next token is a '(', then this is a function expression
		if(p.i + 1 < p.tokens.size() && p.tokens[p.i+1]->getType() == OPEN_PARENTHESIS_TOKEN)
			return parseFunctionExpression(p);
		else
			return parseVariableExpression(p);
		*/
	}
	else if(p.tokens[p.i]->getType() == OPEN_BRACE_TOKEN)
	{
		return parseMapLiteralExpression(p);
	}
	else if(p.tokens[p.i]->getType() == OPEN_SQUARE_BRACKET_TOKEN)
	{
		return parseArrayOrVectorLiteralExpression(p);
	}
	else if(p.tokens[p.i]->getType() == BACK_SLASH_TOKEN)
	{
		return ASTNodeRef(parseAnonFunction(p).getPointer());
	}
	else
	{
		throw LangParserExcep("Expected literal or identifier in expression.");
	}
}


TypeRef LangParser::parseType(const ParseInfo& p, const std::vector<std::string>& generic_type_params)
{
	const std::string t = parseIdentifier("type", p);
	if(t == "float" || t == "real")
		return TypeRef(new Float());
	else if(t == "int")
		return TypeRef(new Int());
	else if(t == "string")
		return TypeRef(new String());
	else if(t == "voidptr")
		return TypeRef(new VoidPtrType());
	else if(t == "bool")
		return TypeRef(new Bool());
	else if(t == "map")
		return parseMapType(p, generic_type_params);
	else if(t == "array")
		return parseArrayType(p, generic_type_params);
	else if(t == "function")
		return parseFunctionType(p, generic_type_params);
	else if(t == "vector")
		return parseVectorType(p, generic_type_params);
	else
	{
		// Then this might be the name of a named type.
		// So look up the named type map
		std::map<std::string, TypeRef>::const_iterator res = p.named_types.find(t);
		if(res == p.named_types.end())
		{
			// Not a named type, maybe it is a type parameter
			for(unsigned int i=0; i<generic_type_params.size(); ++i)
				if(t == generic_type_params[i])
					return TypeRef(new GenericType(i));

			// If it wasn't a generic type, then it's completely unknown, like a rolling stone.
			throw LangParserExcep("Unknown type '" + t + "'.");
		}
		else
		{
			// Type found, return it
			return (*res).second;
		}
	}
}


TypeRef LangParser::parseMapType(const ParseInfo& p, const std::vector<std::string>& generic_type_params)
{
	parseToken(LEFT_ANGLE_BRACKET_TOKEN, p);

	TypeRef from = parseType(p, generic_type_params);

	parseToken(COMMA_TOKEN, p);

	TypeRef to = parseType(p, generic_type_params);

	parseToken(RIGHT_ANGLE_BRACKET_TOKEN, p);

	return TypeRef(new Map(from, to));
}


TypeRef LangParser::parseArrayType(const ParseInfo& p, const std::vector<std::string>& generic_type_params)
{
	parseToken(LEFT_ANGLE_BRACKET_TOKEN, p);

	TypeRef t = parseType(p, generic_type_params);

	parseToken(RIGHT_ANGLE_BRACKET_TOKEN, p);

	return TypeRef(new ArrayType(t));
}


TypeRef LangParser::parseFunctionType(const ParseInfo& p, const std::vector<std::string>& generic_type_params)
{
	parseToken(LEFT_ANGLE_BRACKET_TOKEN, p);

	std::vector<TypeRef> types;

	types.push_back(parseType(p, generic_type_params));

	while(isTokenCurrent(COMMA_TOKEN, p))
	{
		parseToken(COMMA_TOKEN, p);

		types.push_back(parseType(p, generic_type_params));
	}

	parseToken(RIGHT_ANGLE_BRACKET_TOKEN, p);

	std::vector<TypeRef> arg_types;
	for(int i=0; i<(int)types.size() - 1; ++i)
		arg_types.push_back(types[i]);

	return TypeRef(new Function(
		arg_types, 
		types.back(), 
		vector<TypeRef>(), // captured var types
		false // use_captured_vars
	));
}


Reference<StructureType> LangParser::parseStructType(const ParseInfo& p, const std::vector<std::string>& generic_type_params)
{
	const std::string id = parseIdentifier("struct", p);
	assert(id == "struct");

	const std::string name = parseIdentifier("structure name", p);

	parseToken(OPEN_BRACE_TOKEN, p);

	std::vector<TypeRef> types;
	std::vector<string> names;

	types.push_back(parseType(p, generic_type_params));
	names.push_back(parseIdentifier("field name", p));

	while(isTokenCurrent(COMMA_TOKEN, p))
	{
		parseToken(COMMA_TOKEN, p);

		types.push_back(parseType(p, generic_type_params));
		names.push_back(parseIdentifier("field name", p));
	}

	parseToken(CLOSE_BRACE_TOKEN, p);

	return Reference<StructureType>(new StructureType(name, types, names));
}


TypeRef LangParser::parseVectorType(const ParseInfo& p, const std::vector<std::string>& generic_type_params)
{
	parseToken(LEFT_ANGLE_BRACKET_TOKEN, p);

	TypeRef t = parseType(p, generic_type_params);

	parseToken(COMMA_TOKEN, p);

	Reference<IntLiteral> int_literal = parseIntLiteral(p);
	int num = int_literal->value;

	if(num <= 0 || num >= 128 || !Maths::isPowerOfTwo(num))
		throw LangParserExcep("num must be > 0, < 128, and a power of two.");

	parseToken(RIGHT_ANGLE_BRACKET_TOKEN, p);

	return TypeRef(new VectorType(t, num));
}


ASTNodeRef LangParser::parseAddSubExpression(const ParseInfo& p)
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
		if(isTokenCurrent(PLUS_TOKEN, p))
		{
			parseToken(PLUS_TOKEN, p);

			AdditionExpression* addexpr = new AdditionExpression();
			addexpr->a = left;
			addexpr->b = parseMulDivExpression(p);
			
			left = ASTNodeRef(addexpr);
		}
		else if(isTokenCurrent(MINUS_TOKEN, p))
		{
			parseToken(MINUS_TOKEN, p);

			SubtractionExpression* e = new SubtractionExpression();
			e->a = left;
			e->b = parseMulDivExpression(p);
			
			left = ASTNodeRef(e);
		}
		else
		{
			return left;
		}
	}
}


ASTNodeRef LangParser::parseMulDivExpression(const ParseInfo& p)
{
	ASTNodeRef left = parseComparisonExpression(p);
	while(1)
	{
		if(isTokenCurrent(ASTERISK_TOKEN, p))
		{
			parseToken(ASTERISK_TOKEN, p);

			MulExpression* expr = new MulExpression();
			expr->a = left;
			expr->b = parseComparisonExpression(p);
			left = ASTNodeRef(expr);
		}
		else if(isTokenCurrent(FORWARDS_SLASH_TOKEN, p))
		{
			parseToken(FORWARDS_SLASH_TOKEN, p);

			DivExpression* expr = new DivExpression();
			expr->a = left;
			expr->b = parseComparisonExpression(p);
			left = ASTNodeRef(expr);
		}
		else
			return left;
	}
}


ASTNodeRef LangParser::parseComparisonExpression(const ParseInfo& p)
{
	ASTNodeRef left = parseUnaryExpression(p);

	for(unsigned int i=0; i<comparison_tokens.size(); ++i)
	{
		const unsigned int token = comparison_tokens[i];
		if(isTokenCurrent(token, p))
		{
			parseToken(token, p);

			ComparisonExpression* expr = new ComparisonExpression(makeTokenObject(token, p.tokens[p.i - 1]->char_index), left, parseUnaryExpression(p));
			return ASTNodeRef(expr);
		}
	}

	return left;
}


ASTNodeRef LangParser::parseUnaryExpression(const ParseInfo& p)
{
	if(isTokenCurrent(MINUS_TOKEN, p))
	{
		parseToken(MINUS_TOKEN, p);

		UnaryMinusExpression* unary_expr = new UnaryMinusExpression();
		unary_expr->expr = parseUnaryExpression(p);

		return ASTNodeRef(unary_expr);
	}
	else
	{
		return parseParenExpression(p);
	}
}


ASTNodeRef LangParser::parseParenExpression(const ParseInfo& p)
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
}


ASTNodeRef LangParser::parseMapLiteralExpression(const ParseInfo& p)
{
	parseToken(OPEN_BRACE_TOKEN, p);

	MapLiteral* m = new MapLiteral();

	while(1)
	{
		if(isTokenCurrent(CLOSE_BRACE_TOKEN, p)) // if(i < tokens.size() && tokens[i]->getType() == CLOSE_BRACE_TOKEN)
			break;

		// Parse key
		ASTNodeRef key = parseExpression(p);
		
		parseToken(COLON_TOKEN, p);

		// Parse value
		ASTNodeRef value = parseExpression(p);

		m->items.push_back(std::make_pair(key, value));

		if(isTokenCurrent(CLOSE_BRACE_TOKEN, p))//if(p.i < .itokens.size() && tokens[i]->getType() == CLOSE_BRACE_TOKEN)
			break;

		parseToken(COMMA_TOKEN, p);
	}

	parseToken(CLOSE_BRACE_TOKEN, p);

	return ASTNodeRef(m);
}


ASTNodeRef LangParser::parseArrayOrVectorLiteralExpression(const ParseInfo& p)
{
	parseToken(OPEN_SQUARE_BRACKET_TOKEN, p);

	//ArrayLiteral* m = new ArrayLiteral();
	vector<ASTNodeRef> elems;

	while(1)
	{
		if(isTokenCurrent(CLOSE_SQUARE_BRACKET_TOKEN, p)) // if(i < tokens.size() && tokens[i]->getType() == CLOSE_BRACE_TOKEN)
			break;

		// Parse element
		ASTNodeRef elem = parseExpression(p);
		
		elems.push_back(elem);

		if(isTokenCurrent(CLOSE_SQUARE_BRACKET_TOKEN, p))
			break;

		parseToken(COMMA_TOKEN, p);
	}

	parseToken(CLOSE_SQUARE_BRACKET_TOKEN, p);

	//if(isTokenCurrent(IDENTIFIER_TOKEN, p))
	const std::string id = parseIdentifier("square bracket literal suffix", p);
	if(id == "a")
		return ASTNodeRef(new ArrayLiteral(elems));
	else if(id == "v")
		return ASTNodeRef(new VectorLiteral(elems));
	else
		throw LangParserExcep("Unknown square bracket literal suffix '" + id + "'.");
}


Reference<LetASTNode> LangParser::parseLet(const ParseInfo& p)
{
	//parseIdentifier("let", p);

	const std::string var_name = parseIdentifier("variable name", p);

	parseToken(EQUALS_TOKEN, p);

	Reference<LetASTNode> letnode = Reference<LetASTNode>(new LetASTNode(var_name));

	ASTNodeRef expr = parseExpression(p);

	letnode->expr = expr;

	return letnode;
}


FunctionDefinitionRef LangParser::parseAnonFunction(const ParseInfo& p)
{
	parseToken(BACK_SLASH_TOKEN, p);

	const std::string func_name = "anon_func_" + ::toString(p.i);

	FunctionDefinitionRef def = parseFunctionDefinitionGivenName(func_name, p);

	def->use_captured_vars = true;

	// Add this anon function to list of parsed function definitions.
	p.func_defs.push_back(def);
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


void LangParser::parseParameterList(const ParseInfo& p, const std::vector<std::string>& generic_type_params, std::vector<FunctionDefinition::FunctionArg>& args_out)
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

		TypeRef param_type = parseType(p, generic_type_params);
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


const std::string LangParser::errorPosition(const ParseInfo& p)
{
	return errorPosition(p.text_buffer, p.tokens[p.i]->char_index);
}


const std::string LangParser::errorPositionPrevToken(const ParseInfo& p)
{
	return errorPosition(p.text_buffer, p.tokens[p.i - 1]->char_index);
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


} //end namespace Winter
