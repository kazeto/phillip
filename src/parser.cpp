#include <functional>
#include <cassert>
#include <cstring>

#include "./parser.h"


namespace phil
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

condition_t digit = [=](char ch) { return (ch >= '0') and (ch <= '9'); };
condition_t space = is(" \t\n\r");
condition_t quotation = is("\'\"");
condition_t bracket = is("(){}[]<>");
condition_t bad = [=](char ch) { return (ch == -1) or (ch == 0); };

condition_t in_predicate = (not (bad | space | bracket));


formatter_t word(const std::string &w)
{
    return [=](const std::string &str)
    {
        auto len = str.length();

        if (len <= w.length())
            return (std::tolower(str.back()) == w.at(len - 1));
        else
            return space(str.back());
    };
}


formatter_t comment = [=](const std::string &str)
{
    if (str.empty()) return true;
    if (str.front() != '#') return false;

    auto len = str.length();

    if (len <= 1)
        return true;
    else
        return (str.at(len - 2) != '\n');
};


formatter_t string = [=](const std::string &str)
{
    static condition_t cond = (not (bad | space | bracket));
    assert(not str.empty());

    if (quotation(str.front()))
    {
        auto len = str.length();
        if (len > 1)
            return (str.front() != str.at(len - 2));
        else
            return true; // not closed yet
    }
    else
        return cond(str.back);
};




stream_t::stream_t(std::istream *is)
    : std::unique_ptr<std::istream>(is), m_row(1), m_column(1)
{}


stream_t::stream_t(const std::string &path)
    : m_row(1), m_column(1)
{
    std::ifstream *fin = new std::ifstream(path);

    if (*fin)
        reset(fin);
    else
        throw;
}


char stream_t::get(const condition_t &f)
{
    char c = (*this)->get();

    if (c != std::istream::traits_type::eof())
    {
        if (f(c))
        {
            if (c == '\n')
            {
                ++m_row;
                m_column = 1;
            }
            else
                ++m_column;

            return c;
        }
        else
        {
            (*this)->unget();
            return 0;
        }
    }

    return -1;
}


string_t stream_t::read(const condition_t &f)
{
    string_t out;
    char ch = get(f);

    while (not bad(ch))
    {
        out += ch;
        ch = get(f);
    }

    return out;
}


string_t stream_t::read(const formatter_t &f)
{
    string_t out;
    char ch = (*this)->get();

    while (not bad(ch))
    {
        if (not f(out + ch))
        {
            (*this)->unget();
            break;
        }
        else
        {
            out += ch;
            ch = (*this)->get();
        }
    }

    return out;
}


void stream_t::ignore(const condition_t &f)
{
    char ch = get(f);

    while (not bad(ch))
        ch = get(f);
}


parser_t::parser_t(std::istream *is)
    : m_stream(is)
{}


parser_t::parser_t(const std::string &path)
    : m_stream(path)
{}


bool parser_t::read()
{
    string_t line;

    while (not m_stream->eof())
    {
        m_stream.ignore(space);

        if ((bool)(line = m_stream.read(comment)))
            continue;

        if ((bool)(line = m_stream.read(word("problem"))))
        {
        }

        if ((bool)(line = m_stream.read(word("rule"))))
        {
        }

        if ((bool)(line = m_stream.read(word("define"))))
        {
        }
    }
}


}

}