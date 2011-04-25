require 'FileUtils'


def makeHeader(path, classname)

	f = File.new(path, "w")
	
	f << 
<<EOF
/*=====================================================================
#{classname}.h
-------------------
Copyright Nicholas Chapman
Generated at #{Time.now.to_s}
=====================================================================*/
#pragma once


namespace Winter
{


/*=====================================================================
#{classname}
-------------------

=====================================================================*/
class #{classname}
{
public:
	#{classname}();
	~#{classname}();

private:

};


} // end namespace Winter

EOF

f.close

end


def makeCPP(path, classname)

	f = File.new(path, "w")
	
	f << 
<<EOF
/*=====================================================================
#{classname}.cpp
-------------------
Copyright Nicholas Chapman
Generated at #{Time.now.to_s}
=====================================================================*/
#include "wnt_#{classname}.h"


namespace Winter
{


#{classname}::#{classname}()
{

}


#{classname}::~#{classname}()
{

}


} // end namespace Winter

EOF

f.close

end

#puts $*.inspect

if $*.length < 2
	puts("Usage: ruby makeclass.rb directory classname")
	exit(1)
end

directory = $*[0]
classname = $*[1]

header_path = File.join(directory, "wnt_" + classname + ".h")
cpp_path = File.join(directory, "wnt_" + classname + ".cpp")

if File.exist?(header_path)
	puts "Error, '#{header_path}' already exists"
	exit(1)
end
if File.exist?(cpp_path)
	puts "Error, '#{cpp_path}' already exists"
	exit(1)
end

makeHeader(header_path, classname)
makeCPP(cpp_path, classname)

puts "Written '#{header_path}'"
puts "Written '#{cpp_path}'"

puts "----------Completed Successfully----------------"
