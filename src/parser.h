#pragma once

#include <functional>
#include <list>
#include <memory>
#include "./define.h"

namespace phil
{

namespace parse
{


using condition_t = std::function<bool(char)>;
using formatter_t = std::function<bool(const std::string&)>;


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
    
    bool read();

private:
    stream_t m_stream;
};





} // end of parse

} // end of phil