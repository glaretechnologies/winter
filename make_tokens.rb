# Copyright 2009 Nicholas Chapman

tokens = [

# token name,			type,			function name,	precedence
#[ "REAL_LITERAL",		:literal,		nil,			nil			],	
#[ "INT_LITERAL",		:literal,		nil,			nil			],	
#[ "STRING_LITERAL",	:literal,		nil,			nil			],	
#[ "BOOLEAN_LITERAL",	:literal,		nil,			nil			],	
#[ "IDENTIFIER",		:identifier,	nil,			nil			],	
[ "COMMA",				:nil,			nil,			nil			],	
[ "OPEN_PARENTHESIS",	:paranthesis,	nil,			nil			],	
[ "CLOSE_PARENTHESIS",	:paranthesis,	nil,			nil			],	
[ "OPEN_BRACE",			:nil,			nil,			nil			],	
[ "CLOSE_BRACE",		:nil,			nil,			nil			],	
[ "OPEN_SQUARE_BRACKET",:nil,			nil,			nil			],	
[ "CLOSE_SQUARE_BRACKET",:nil,			nil,			nil			],	

[ "COLON",				:nil,			nil,			nil			],	
[ "RIGHT_ARROW",		:nil,			nil,			nil			],	
[ "EQUALS",				:nil,			nil,			nil			],	
[ "PLUS",				:bin_infix,		"add",			100			],	
[ "MINUS",				:bin_infix,		"sub",			100			],	
[ "FORWARDS_SLASH",		:bin_infix,		"div",			200			],	
[ "BACK_SLASH",			:nil,			nil,			nil			],	
[ "ASTERISK",			:bin_infix,		"mul",			200			],	
[ "LEFT_ANGLE_BRACKET",	:bin_infix,		"lt",			60			],	
[ "RIGHT_ANGLE_BRACKET",:bin_infix,		"gt",			60			],	
[ "LESS_EQUAL",			:bin_infix,		"lte",			60			],	
[ "GREATER_EQUAL",		:bin_infix,		"gte",			60			],	
[ "DOUBLE_EQUALS",		:bin_infix,		"eq",			50			],	
[ "NOT_EQUALS",			:bin_infix,		"neq",			50			],	
[ "AND",				:bin_infix,		"and",			40			],	
[ "OR",					:bin_infix,		"or",			30			],	

]




def write_token(token, file, num)

	
	file.puts ""
	file.puts "const unsigned int #{token[0]}_TOKEN = #{num};"
	#$i += 1
	
	file.puts "class #{token[0]}_Token : public TokenBase"
	file.puts "{"
	file.puts "public:"
	
		file.puts " #{token[0]}_Token(unsigned int char_index) : TokenBase(char_index, #{token[0]}_TOKEN) {}"
		
		file.puts " virtual bool isLiteral() const { return #{token[1] == :literal ? "true" : "false"}; }"
		
		# file.puts " virtual bool isIdentifier() const { return #{token[1] == :identifier ? "true" : "false"}; }"
		
		file.puts " virtual bool isParanthesis() const { return #{token[1] == :paranthesis ? "true" : "false"}; }"

		file.puts " virtual bool isBinaryInfixOp() const { return #{token[1] == :bin_infix ? "true" : "false"}; }"

		file.puts " virtual const std::string functionName() const { return \"#{token[2]}\"; }" if token[2]

		file.puts " virtual int precedence() const { return #{token[3]}; }" if token[3]

	file.puts "};"

end

file = File.new("GeneratedTokens.h", "w")
file.puts "// Autogenerated by make_tokens.rb on #{Time.now}"
file.puts

$i = 10
tokens.each do |t|
	write_token(t, file, $i)
	$i += 1
end

file.puts "\n\ninline const std::string tokenName(unsigned int t)"
file.puts '{'
file.puts "\tswitch(t) {"
$i = 10
tokens.each do |t|
	file.puts "\t\tcase #{$i}: return \"#{t[0]}\";"
	$i += 1
end
file.puts "\t\tdefault: return \"[Unknown]\";"
file.puts "\t}"
file.puts "}"

