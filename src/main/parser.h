#pragma once

#include <functional>
#include <list>
#include <memory>

#include "./fol.h"

namespace dav
{

namespace parse
{


enum format_result_e
{
	FMT_BAD,
	FMT_READING,
	FMT_GOOD,
};

using condition_t = std::function<bool(char)>;
using formatter_t = std::function<format_result_e(const std::string&)>;

condition_t operator&(const condition_t &c1, const condition_t &c2);
condition_t operator|(const condition_t &c1, const condition_t &c2);
condition_t operator!(const condition_t &c);
condition_t is(char t);
condition_t is(const std::string &ts);

extern condition_t lower;
extern condition_t upper;
extern condition_t alpha;
extern condition_t digit;
extern condition_t space;
extern condition_t quotation_mark;
extern condition_t bracket;
extern condition_t newline;
extern condition_t bad;
extern condition_t is_general;

formatter_t operator&(const formatter_t &f1, const formatter_t &f2);
formatter_t operator|(const formatter_t &f1, const formatter_t &f2);
formatter_t word(const std::string &w);
formatter_t many(const condition_t &c);
formatter_t startswith(const condition_t &c);
formatter_t enclosed(char begin, char last);

extern formatter_t quotation;
extern formatter_t comment;
extern formatter_t general;
extern formatter_t argument;
extern formatter_t parameter;
extern formatter_t name;
extern formatter_t predicate;


/** A wrapper class of input-stream. */
class stream_t : public std::unique_ptr<std::istream>
{
public:
	/** @param ptr The pointer of an input-stream allocated by `new`. */
    stream_t(std::istream *ptr);
    stream_t(const filepath_t &path);

    char get(const condition_t&);
	bool peek(const condition_t&) const;

    string_t read(const formatter_t&);

    void ignore(const condition_t&);

    void skip(); /// Skips spaces and comments.

    size_t row() const { return m_row; }
    size_t column() const { return m_column; }

	exception_t exception(const string_t&) const;

private:
    size_t m_row, m_column;
};


/** A class to manage parsing. */
class parser_t
{
public:
    parser_t(std::istream *is);
    parser_t(const std::string &path);
    
    void read();
	bool eof() const { return m_stream->eof(); }

	const std::unique_ptr<problem_t>& prob() { return m_problem; }
	const std::unique_ptr<rule_t>& rule() { return m_rule; }
	const std::unique_ptr<predicate_property_t>& prop() { return m_property; }

private:
    stream_t m_stream;

	std::unique_ptr<problem_t> m_problem;
	std::unique_ptr<rule_t> m_rule;
	std::unique_ptr<predicate_property_t> m_property;
};


} // end of parse

} // end of dav