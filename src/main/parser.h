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


/// A wrapper class of input-stream.
class stream_t : public std::unique_ptr<std::istream>
{
public:
    stream_t(std::istream*);
    stream_t(const std::string &path);

    char get(const condition_t&);

    string_t read(const formatter_t&);

    void ignore(const condition_t&);

    void skip(); /// Skips spaces and comments.

    size_t row() const { return m_row; }
    size_t column() const { return m_column; }

private:
    size_t m_row, m_column;
};


/// A class of results of parsing.
struct node_t
{
    string_t str;
};


/// A class to manage parsing.
class parser_t
{
public:
    parser_t(std::istream *is);
    parser_t(const std::string &path);
    
    void read();
	bool eof() const { return m_stream->eof(); }

private:
    stream_t m_stream;

	std::unique_ptr<problem_t> m_problem;
	std::unique_ptr<rule_t> m_rule;
	std::unique_ptr<predicate_property_t> m_property;
};


} // end of parse

} // end of dav