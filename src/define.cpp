/* -*- coding: utf-8 -*- */

#include <cstring>
#include <cassert>
#include <errno.h>

#include "./define.h"
#include "./sexp.h"

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

const int FAILURE_MKDIR = -1;


namespace phil
{


bool string_t::to_arity(predicate_t *p, arity_t *n) const
{
    int idx = rfind('/');
    if (idx != std::string::npos)
    {
        if (p != nullptr)
            (*p) = substr(0, idx);
        if (n != nullptr)
            (*n) = std::stoi(substr(idx + 1));
        return true;
    }
    else
        return false;
}


std::pair<predicate_t, arity_t> string_t::to_arity() const
{
    std::pair<predicate_t, arity_t> out{ "", 0 };
    to_arity(&out.first, &out.second);
    return out;
}


string_t string_t::to_lower() const
{
    string_t out(*this);
    for (auto &c : out) c = std::tolower(c);
    return out;
}


std::vector<string_t> string_t::split(const char *separator, const int MAX_NUM) const
{
    auto _find_split_index = [this](const char *separator, int begin) -> index_t
    {
        for (size_t i = begin; i < size(); ++i)
        {
            if (strchr(separator, at(i)) != NULL)
                return static_cast<int>(i);
        }
        return -1;
    };

    std::vector<string_t> out;
    int idx(0);

    while (idx < size())
    {
        int idx2(_find_split_index(separator, idx));

        if (idx2 < 0)
            idx2 = size();

        if (idx2 - idx > 0)
        {
            if (MAX_NUM > 0 and out.size() >= MAX_NUM)
                idx2 = size();
            out.push_back(substr(idx, idx2 - idx));
        }

        idx = idx2 + 1;
    }

    return out;
}


string_t string_t::replace(const std::string &from, const std::string &to) const
{
    size_t pos(0);
    string_t out(*this);

    if (from.empty()) return out;

    while((pos = out.find(from, pos)) != std::string::npos)
    {
        out.std::string::replace(pos, from.length(), to);
        pos += to.length();
    }

    return out;
}


std::mutex string_hash_t::ms_mutex_hash;
std::mutex string_hash_t::ms_mutex_unknown;
hash_map<std::string, unsigned> string_hash_t::ms_hashier;
std::vector<std::string> string_hash_t::ms_strs;
unsigned string_hash_t::ms_issued_variable_count = 0;


literal_t::literal_t(const sexp::sexp_t &s)
    : truth(true)
{
    if (s.is_functor())
    {
        const std::string &&str = s.child(0).string();
        if (str.at(0) == '!')
        {
            truth = false;
            predicate = predicate_t(str.substr(1));
        }
        else
            predicate = predicate_t(str);

        for (auto it = ++s.children().cbegin(); it != s.children().cend(); ++it)
        {
            if (not (*it)->is_parameter())
            {
                term_t term((*it)->string());
                terms.push_back(term);
            }
        }
    }
    else
        predicate = s.child(0).string();

    if (predicate.length() >= 255)
    {
        util::print_warning_fmt(
            "Following predicate is too long and shortened: \"%s\"",
            predicate.c_str());
        predicate = predicate.substr(0, 250);
    }

    regularize();
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
            util::format( "\33[40m%s\33[0m", predicate.c_str() );
    else
        (*p_out_str) += predicate;
#endif

    for( int i=0; i<terms.size(); i++ )
    {
        (*p_out_str) += " ";
#ifdef _WIN32
        (*p_out_str) += terms[i].string();
#else
        if (f_colored)
        {
            const int &_color = color[(terms[i].get_hash()) % 8];
            (*p_out_str) += util::format(
                "\33[0;%dm%s\33[0m",
                _color, terms[i].string().c_str() );
        }
        else
            (*p_out_str) += terms[i].string();
#endif
    }
    (*p_out_str) += ")";
}


size_t literal_t::write_binary(char *bin) const
{
    size_t n(0);

    n += util::string_to_binary(predicate, bin);

    /* terms */
    n += util::num_to_binary(terms.size(), bin + n);
    for (int i = 0; i < terms.size(); ++i)
        n += util::string_to_binary(terms.at(i).string(), bin + n);

    n += util::bool_to_binary(truth, bin + n);

    return n;
}


size_t literal_t::read_binary( const char *bin )
{
    size_t n(0);
    std::string s_buf;
    int i_buf;

    n += util::binary_to_string(bin, &s_buf);
    predicate = predicate_t(s_buf);

    n += util::binary_to_num(bin + n, &i_buf);
    terms.assign(i_buf, term_t());
    for( int i=0; i<i_buf; ++i )
    {
        n += util::binary_to_string(bin + n, &s_buf);
        terms[i] = term_t(s_buf);
    }

    n += util::binary_to_bool(bin + n, &truth);

    return n;
}


std::ostream& operator<<(std::ostream& os, const term_t& t)
{
    return os << t.string();
}


std::ostream& operator<<(std::ostream& os, const literal_t& lit)
{
    return os << lit.to_string();
}



namespace util
{


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
            throw phil::phillip_exception_t(
            "Failed to open a database file: " + m_filename);
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
            throw phillip_exception_t(
            "Failed to open a database file: " + m_filename);
        else
        {
            m_finder = new cdbpp::cdbpp(*m_fin);

            if (not m_finder->is_open())
                throw phillip_exception_t(
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


void xml_element_t::print(std::ostream *os) const
{
    std::function<void(const xml_element_t&)>
        elem_to_string = [&](const xml_element_t &e) -> void
    {
        auto attr_to_string = [](const std::pair<std::string, std::string> &p) -> std::string
        { return format("%s=\"%s\"", p.first.c_str(), p.second.c_str()); };

        (*os) << "<" << e.name() << " ";
        (*os) << join_f(e.attributes(), attr_to_string, " ") << ">" << std::endl;

        if (not e.text().empty())
            (*os) << e.text() << std::endl;

        for (auto c : e.children())
            elem_to_string(c);

        (*os) << "</" << e.name() << ">" << std::endl;
    };

    elem_to_string(*this);
}


const int BUFFER_SIZE_FOR_FMT = 256 * 256;

struct { int year, month, day, hour, minuite, second; } TIME_BEGIN;


void initialize()
{
    now(&TIME_BEGIN.year, &TIME_BEGIN.month, &TIME_BEGIN.day,
        &TIME_BEGIN.hour, &TIME_BEGIN.minuite, &TIME_BEGIN.second);
}


duration_time_t duration_time(const std::chrono::system_clock::time_point &begin)
{
    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - begin);

    return duration.count() / 1000.0f;
}


void print_console_fmt(const char *format, ...)
{
    char buffer[BUFFER_SIZE_FOR_FMT];
    va_list arg;
    va_start(arg, format);
#ifdef _WIN32
    vsprintf_s(buffer, BUFFER_SIZE_FOR_FMT, format, arg);
#else
    vsprintf(buffer, format, arg);
#endif
    va_end(arg);

    print_console(buffer);
}


void print_error_fmt(const char *format, ...)
{
    char buffer[BUFFER_SIZE_FOR_FMT];
    va_list arg;
    va_start(arg, format);
#ifdef _WIN32
    vsprintf_s(buffer, BUFFER_SIZE_FOR_FMT, format, arg);
#else
    vsprintf(buffer, format, arg);
#endif
    va_end(arg);

    print_error(buffer);
}


void print_warning_fmt(const char *format, ...)
{
    char buffer[BUFFER_SIZE_FOR_FMT];
    va_list arg;
    va_start(arg, format);
#ifdef _WIN32
    vsprintf_s(buffer, BUFFER_SIZE_FOR_FMT, format, arg);
#else
    vsprintf(buffer, format, arg);
#endif
    va_end(arg);

    print_warning(buffer);
}


void beginning_time(
    int *year, int *month, int *day, int *hour, int *min, int *sec)
{
    *year = TIME_BEGIN.year;
    *month = TIME_BEGIN.month;
    *day = TIME_BEGIN.day;
    *hour = TIME_BEGIN.hour;
    *min = TIME_BEGIN.minuite;
    *sec = TIME_BEGIN.second;
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
    int year, month, day, hour, min, sec;
    now(&year, &month, &day, &hour, &min, &sec);

#ifdef _WIN32
    return format(
        "# %02d/%02d/%04d %02d:%02d:%02d | ",
        month, day, year, hour, min, sec);
#else
    time_t t;
    tm *p_ltm;
    time(&t);
    p_ltm = localtime(&t);

    return format(
        "\33[0;34m# %02d/%02d/%04d %02d:%02d:%02d\33[0m] ",
        month, day, year, hour, min, sec);
#endif
}


std::vector<std::string> split(
    const std::string &str, const char *separator, const int MAX_NUM )
{
    auto _find_split_index = [](
        const std::string &str, const char *separator, int begin) -> index_t
    {
        for (size_t i = begin; i<str.size(); ++i)
        {
            if (strchr(separator, str.at(i)) != NULL)
                return static_cast<int>(i);
        }
        return -1;
    };

    std::vector<std::string> out;
    int idx(0);

    while (idx < str.size())
    {
        int idx2(_find_split_index(str, separator, idx));

        if (idx2 < 0)
            idx2 = str.size();

        if (idx2 - idx > 0)
        {
            if (MAX_NUM > 0 and out.size() >= MAX_NUM)
                idx2 = str.size();
            out.push_back(str.substr(idx, idx2 - idx));
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
    if (query.size() <= str.size())
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
    if (query.size() <= str.size())
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


void mkdir(std::string path)
{
    auto makedir = [](const std::string path) -> bool
    {
#ifdef _WIN32
        if (::_mkdir(path.c_str()))
            return true;
#else
        if (::mkdir(path.c_str(), 0755))
            return true;
#endif
        else
            return (errno == EEXIST);
    };

#ifdef _WIN32
    const std::string det = "\\";
#else
    const std::string det = "/";
#endif
    
    auto splitted = split(path, det.c_str());
    path = (path[0] == det[0]) ? det : "";

    for (auto it = splitted.begin(); it != splitted.end(); ++it)
    {
        if (it != splitted.begin())
        {
#ifdef _WIN32
            path += '\\';
#else
            path += '/';
#endif
        }
            
        path += (*it);

        if (not path.empty())
        if (not makedir(path))
        {
            print_error_fmt("Failed to make directory \"%s\"", path.c_str());
            return;
        }
    }
}


std::string reguralize_path(const std::string &target)
{
    std::string out(target);

    for (int i = 0; i < out.length(); ++i)
    {
#ifdef _WIN32
        if (out.at(i) == '/')
            out[i] = '\\';
#else
        if (out.at(i) == '\\')
            out[i] = '/';
#endif
    }

    if (out.find("%TIME") >= 0)
    {
        int year, month, day, hour, minute, second;
        beginning_time(&year, &month, &day, &hour, &minute, &second);

        std::string _replace = format(
            "%04d%02d%02d_%02d%02d%02d",
            year, month, day, hour, minute, second);
        out = replace(out, "%TIME", _replace);
    }
    if (out.find("%DAY") >= 0)
    {
        int year, month, day, hour, minute, second;
        beginning_time(&year, &month, &day, &hour, &minute, &second);

        std::string _replace = format("%04d%02d%02d", year, month, day);
        out = replace(out, "%DAY", _replace);
    }

    return out;
}


std::string indexize_path(std::string str, int idx)
{
    if (str.empty()) return "";

    std::string rep = format("_%d", idx);
    for (int i = str.size() - 1; i >= 0; --i)
    {
        if (str.at(i) == '.')
            return str.substr(0, i) + rep + str.substr(i);            
        if (str.at(i) == '/' or str.at(i) == '\\')
            return str + rep;
    }

    return str + rep;
}


bool parse_string_as_function_call(
    const std::string &str, std::string *pred, std::vector<std::string> *terms)
{
    int num_open(0), num_close(0);
    int idx_open(-1), idx_close(-1);
    std::list<int> commas;

    for (int i = 0; i < str.size(); ++i)
    {
        char c = str.at(i);

        if (c == '(')
        {
            ++num_open;
            if (num_open == 1)
                idx_open = i;
        }
        
        if (c == ')')
        {
            ++num_close;
            if (num_open == num_close)
                idx_close = i;
            if (num_open < num_close)
                return false; // INVALID FORMAT
        }

        if (c == ',')
        {
            if (num_open == num_close + 1)
                commas.push_back(i);
        }
    }

    if (idx_open >= 0 and idx_close >= 0)
    {
        (*pred) = util::strip(str.substr(0, idx_open), " ");
        terms->clear();

        if (commas.empty())
        {
            if (idx_close - idx_open > 1)
            {
                std::string s = util::strip(str.substr(idx_open + 1, idx_close - idx_open - 1), " ");
                if (not s.empty())
                    terms->push_back(s);
            }
        }
        else
        {
            terms->push_back(
                util::strip(str.substr(idx_open + 1, commas.front() - idx_open - 1), " "));
            for (auto it = commas.begin(); std::next(it) != commas.end(); ++it)
                terms->push_back(
                util::strip(str.substr((*it) + 1, (*std::next(it)) - (*it) - 1), " "));
            terms->push_back(
                util::strip(str.substr(commas.back() + 1, idx_close - commas.back() - 1), " "));
        }
    }

    if (idx_open < 0 and idx_close < 0)
        (*pred) = str;

    if (pred->empty()) return false;
    for (auto t : (*terms)) if (t.empty()) return false;

    return true;
}


std::mutex g_mutex_for_print;


} // end of util

} // end of phil
