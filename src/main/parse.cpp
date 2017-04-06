#include <functional>
#include <algorithm>
#include <cassert>
#include <cstring>

#include "./parse.h"


namespace dav
{

namespace parse
{


condition_t operator&(const condition_t &c1, const condition_t &c2)
{
    return [=](char ch) { return c1(ch) and c2(ch); };
}

condition_t operator|(const condition_t &c1, const condition_t &c2)
{
    return [=](char ch) { return c1(ch) or c2(ch); };
}

condition_t operator!(const condition_t &c)
{
    return [=](char ch) {return not c(ch); };
}

condition_t is(char t)
{
    return [=](char ch) { return ch == t; };
}

condition_t is(const std::string &ts)
{
    return [=](char ch)
    {
        for (auto t : ts)
            if (ch == t) return true;
        return false;
    };
}

condition_t lower = [=](char ch) { return (ch >= 'a') and (ch <= 'z'); };
condition_t upper = [=](char ch) { return (ch >= 'A') and (ch <= 'Z'); };
condition_t alpha = lower | upper;
condition_t digit = [=](char ch) { return (ch >= '0') and (ch <= '9'); };
condition_t space = is(" \t\n\r");
condition_t quotation_mark = is("\'\"");
condition_t bracket = is("(){}[]<>");
condition_t newline = is('\n');
condition_t bad = [=](char ch) { return (ch == -1) or (ch == 0); };
condition_t is_general = not (bad | space | bracket | quotation_mark | is("#^!|="));


formatter_t operator&(const formatter_t &f1, const formatter_t &f2)
{
    return [=](const std::string &s)
	{
		return static_cast<format_result_e>(std::min<int>(f1(s), f2(s)));
	};
}

formatter_t operator|(const formatter_t &f1, const formatter_t &f2)
{
    return [=](const std::string &s)
	{
		return static_cast<format_result_e>(std::max<int>(f1(s), f2(s)));
	};
}


formatter_t word(const std::string &w)
{
    return [=](const std::string &str)
    {
		if (str.empty()) return FMT_READING;

        auto len = str.length();

		if (len > w.length())
			return FMT_BAD;
		else 
		{
			if (str.back() == w.at(len - 1))
				return (len == w.length()) ? FMT_GOOD : FMT_READING;
			else
				return FMT_BAD;
		}
    };
}


formatter_t many(const condition_t &c)
{
    return [=](const std::string &str)
	{
		if (str.empty()) return FMT_READING;
		else return c(str.back()) ? FMT_GOOD : FMT_BAD;
	};
}


formatter_t startswith(const condition_t &c)
{
    return [=](const std::string &str)
	{
		if (str.empty()) return FMT_READING;
		else return (c(str.front())) ? FMT_GOOD : FMT_BAD;
	};
}


formatter_t enclosed(char begin, char last)
{
    return [=](const std::string &str)
    {
		if (str.empty()) return FMT_READING;

		auto len = str.length();

        if (bad(str.back())) return FMT_BAD;

		if (str.front() != begin) return FMT_BAD;
		else
		{
			auto i = str.find(last, 1);

			if (i == std::string::npos) return FMT_READING;
			else if (i == len - 1)      return FMT_GOOD;
			else                        return FMT_BAD;
		}
    };
}


formatter_t quotation = many(not newline) & (enclosed('\'', '\'') | enclosed('\"', '\"'));
formatter_t comment = enclosed('#', '\n');
formatter_t general = many(is_general);
formatter_t argument = (startswith(alpha) & many(alpha | digit)) | quotation;
formatter_t parameter = general | quotation;
formatter_t name = general;
formatter_t predicate = general;


}

}