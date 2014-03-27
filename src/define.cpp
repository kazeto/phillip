/* -*- coding: utf-8 -*- */


#include <cstring>
#include "./define.h"


namespace phil
{

hash_map<std::string, unsigned> string_hash_t::ms_hashier;
std::vector<std::string>        string_hash_t::ms_strs;
unsigned                        string_hash_t::ms_issued_variable_count;


literal_t::literal_t(const sexp::stack_t &s)
    : truth(true)
{
    if (s.is_functor())
    {
        const std::string &str = s.children[0]->children[0]->str;
        if (str.at(0) == '!')
        {
            truth = false;
            predicate = predicate_t(str.substr(1));
        }
        else
            predicate = predicate_t(str);

        for (int i = 1; i < s.children.size(); i++)
        {
            if (not s.children[i]->is_parameter())
            {
                term_t term(s.children[i]->get_string());
                terms.push_back(term);
            }
        }
    }
    else
        predicate = s.children[0]->str;
}


bool literal_t::operator > (const literal_t &x) const
{
    if (truth != x.truth) return truth;
    if (predicate != x.predicate) return (predicate > x.predicate);
    if (terms.size() != x.terms.size())
        return (terms.size() > x.terms.size());

    for (size_t i = 0; i < terms.size(); i++)
    {
        if (terms[i] != x.terms[i])
            return terms[i] > x.terms[i];
    }
    return false;
}


bool literal_t::operator < (const literal_t &x) const
{
    if (truth != x.truth) return not truth;
    if (predicate != x.predicate) return (predicate < x.predicate);
    if (terms.size() != x.terms.size())
        return (terms.size() < x.terms.size());

    for (size_t i = 0; i < terms.size(); i++)
    {
        if (terms[i] != x.terms[i])
            return terms[i] < x.terms[i];
    }
    return false;
}


bool literal_t::operator == (const literal_t &x) const
{
    if (truth != x.truth) return false;
    if (predicate != x.predicate) return false;
    if (terms.size() != x.terms.size()) return false;

    for (size_t i = 0; i < terms.size(); i++)
    {
        if (terms[i] != x.terms[i])
            return false;
    }
    return true;
}


bool literal_t::operator != (const literal_t &x) const
{
    if (truth != x.truth) return true;
    if (predicate != x.predicate) return true;
    if (terms.size() != x.terms.size()) return true;

    for (size_t i = 0; i < terms.size(); i++)
    {
        if (terms[i] != x.terms[i])
            return true;
    }
    return false;
}


/** Get string-expression of the literal. */
void literal_t::print( std::string *p_out_str, bool f_colored ) const
{
    static const int color[] = {31, 32, 33, 34, 35, 36, 37, 38, 39, 40};

    (*p_out_str) += "(";
    if( not truth ) (*p_out_str) += "!";

#ifdef _WIN32
    (*p_out_str) += predicate;
#else
    if( f_colored )
        (*p_out_str) +=
            format( "\33[40m%s\33[0m", predicate.c_str() );
    else
        (*p_out_str) += predicate;
#endif

    for( int i=0; i<terms.size(); i++ )
    {
#ifdef _WIN32
        (*p_out_str) += " " + terms[i].string();
#else
        if( f_colored )
        {
            const int &_color = color[(terms[i].get_hash()) % 8];
            (*p_out_str) +=
                format(
                    "\33[0;%dm%s\33[0m",
                    _color, terms[i].string().c_str() );
        }
        else
            (*p_out_str) += terms[i].string();
#endif
    }
    (*p_out_str) += ")";
}


size_t literal_t::write_binary( char *bin ) const
{
    size_t n(0);
    
    n += string_to_binary( predicate, bin );

    /* terms */
    n += num_to_binary( terms.size(), bin+n );
    for( int i=0; i<terms.size(); ++i )
        n += string_to_binary( terms.at(i).string(), bin+n );

    n += bool_to_binary( truth, bin+n );

    return n;
}


size_t literal_t::read_binary( const char *bin )
{
    size_t n(0);
    std::string s_buf;
    int i_buf;

    n += binary_to_string( bin, &s_buf );
    predicate = predicate_t(s_buf);

    n += binary_to_num( bin+n, &i_buf );
    terms.assign( i_buf, term_t() );
    for( int i=0; i<i_buf; ++i )
    {
        n += binary_to_string( bin+n, &s_buf );
        terms[i] = term_t(s_buf);
    }

    n += binary_to_bool( bin+n, &truth );

    return n;
}


cdb_data_t::cdb_data_t(std::string _filename)
    : m_filename(_filename), m_fout(NULL), m_fin(NULL),
      m_builder(NULL), m_finder(NULL)
{}


cdb_data_t::~cdb_data_t()
{
    finalize();
}


void cdb_data_t::prepare_compile()
{
    if (is_readable()) finalize();

    if (not is_writable())
    {
        m_fout = new std::ofstream(
            m_filename.c_str(),
            std::ios::binary | std::ios::trunc);
        if (m_fout->fail())
        {
            print_error(
                "Failed to open a database file: " + m_filename);
        }
        else
            m_builder = new cdbpp::builder(*m_fout);
    }
}


void cdb_data_t::prepare_query()
{
    if (is_writable()) finalize();

    if (not is_readable())
    {
        m_fin = new std::ifstream(
            m_filename.c_str(), std::ios_base::binary);
        if (m_fin->fail())
        {
            print_error(
                "Failed to open a database file: " + m_filename);
        }
        else
        {
            m_finder = new cdbpp::cdbpp(*m_fin);
            if (not m_finder->is_open())
                print_error(
                "Failed to read a database file: " + m_filename);

        }
    }
}


void cdb_data_t::finalize()
{
    if (m_builder != NULL)
    {
        delete m_builder;
        m_builder = NULL;
    }
    if (m_fout != NULL)
    {
        delete m_fout;
        m_fout = NULL;
    }

    if (m_finder != NULL)
    {
        delete m_finder;
        m_finder = NULL;
    }

    if (m_fin != NULL)
    {
        delete m_fin;
        m_fin = NULL;
    }
}


const int BUFFER_SIZE_FOR_FMT = 256 * 256;
char g_buffer_for_fmt[BUFFER_SIZE_FOR_FMT];


void print_console_fmt(const char *format, ...)
{
    va_list arg;
    va_start(arg, format);
#ifdef _WIN32
    vsprintf_s(g_buffer_for_fmt, BUFFER_SIZE_FOR_FMT, format, arg);
#else
    vsprintf(g_buffer_for_fmt, format, arg);
#endif
    va_end(arg);

    print_console(g_buffer_for_fmt);
}


void print_error_fmt(const char *format, ...)
{
    va_list arg;
    va_start(arg, format);
#ifdef _WIN32
    vsprintf_s(g_buffer_for_fmt, BUFFER_SIZE_FOR_FMT, format, arg);
#else
    vsprintf(g_buffer_for_fmt, format, arg);
#endif
    va_end(arg);

    print_error(g_buffer_for_fmt);
}


void print_warning_fmt(const char *format, ...)
{
    va_list arg;
    va_start(arg, format);
#ifdef _WIN32
    vsprintf_s(g_buffer_for_fmt, BUFFER_SIZE_FOR_FMT, format, arg);
#else
    vsprintf(g_buffer_for_fmt, format, arg);
#endif
    va_end(arg);

    print_warning(g_buffer_for_fmt);
}


std::string format(const char *format, ...)
{
    static const int SIZE = 256 * 256;
    char buffer[SIZE];

    va_list arg;
    va_start(arg, format);
#ifdef _WIN32
    vsprintf_s(buffer, SIZE, format, arg);
#else
    vsprintf(buffer, format, arg);
#endif
    va_end(arg);

    return std::string(buffer);
}


std::string time_stamp()
{
#ifdef _WIN32
    time_t t;
    struct tm ltm;
    time( &t );
    localtime_s( &ltm, &t );

    return format(
        "# %02d/%02d/%04d %02d:%02d:%02d | ",
        1+ltm.tm_mon, ltm.tm_mday, 1900+ltm.tm_year,
        ltm.tm_hour, ltm.tm_min, ltm.tm_sec );
#else
    time_t t;
    tm *p_ltm;
    time( &t );
    p_ltm = localtime( &t );

    return format(
        "\33[0;34m# %02d/%02d/%04d %02d:%02d:%02d\33[0m] ",
        1+p_ltm->tm_mon, p_ltm->tm_mday, 1900+p_ltm->tm_year,
        p_ltm->tm_hour, p_ltm->tm_min, p_ltm->tm_sec );
#endif
}


int _find_split_index(
    const std::string &str, const char *separator, int begin=0 )
{
    for( size_t i=begin; i<str.size(); ++i )
    {
        if( strchr(separator, str.at(i)) != NULL )
            return static_cast<int>(i);
    }
    return -1;
}


std::vector<std::string> split(
    const std::string &str, const char *separator, const int MAX_NUM )
{
    std::vector<std::string> out;
    int idx(0);
    while( idx < str.size() )
    {
        int idx2( _find_split_index(str, separator, idx) );

        if (idx2 < 0)
            idx2 = str.size();

        if (idx2 - idx > 0)
        {
            if (MAX_NUM > 0 and out.size() >= MAX_NUM)
                idx2 = str.size();
            out.push_back( str.substr(idx, idx2-idx) );
        }

        idx = idx2 + 1;
    }
    return out;
}


std::string replace(
    const std::string &input,
    const std::string &find,
    const std::string &replace )
{
    size_t pos(0), find_len(find.length()), replace_len(replace.length());
    std::string ret = input;
    if( find_len == 0 )
        return ret;
    for( ; (pos = ret.find(find, pos)) != std::string::npos; )
    {
        ret.replace(pos, find_len, replace);
        pos += replace_len;
    }
    return ret;
}


std::string strip(const std::string &input, const char *targets)
{
    size_t idx1(0);
    for (; idx1 < input.length(); ++idx1)
        if(strchr(targets, input.at(idx1)) == NULL)
            break;

    size_t idx2(input.length());
    for (; idx2 > idx1; --idx2)
        if(strchr(targets, input.at(idx2 - 1)) == NULL)
            break;

    return (idx1 == idx2) ? "" : input.substr(idx1, idx2 - idx1);
}


bool startswith(const std::string &str, const std::string &query)
{
    if (query.size() >= str.size())
    {
        for (int i = 0; i < query.size(); ++i)
        {
            if (query.at(i) != str.at(i))
                return false;
        }
        return true;
    }
    else
        return false;
}


bool endswith(const std::string &str, const std::string &query)
{
    if (query.size() >= str.size())
    {
        int q = query.size() - 1;
        int s = query.size() - 1;
        for (int i = 0; i < query.size(); ++i)
        {
            if (query.at(q - i) != str.at(s - i))
                return false;
        }
        return true;
    }
    else
        return false;
}


} // end phil
