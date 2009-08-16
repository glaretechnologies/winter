#pragma once


namespace Winter
{


class BaseException
{
public:
	BaseException(const std::string& s_) : s(s_) {}
	const std::string& what() const { return s; }
private:
	std::string s;
};


}
