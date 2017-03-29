/* -*- coding: utf-8 -*- */

#include <cstring>
#include <cassert>
#include <errno.h>

#include "./util.h"
// #include "./kb.h"

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

const int FAILURE_MKDIR = -1;


namespace dav
{







std::ostream& operator<<(std::ostream& os, const string_hash_t& t)
{
    return os << t.string();
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
            throw phillip_exception_t(
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


duration_time_t time_watcher_t::duration() const
{
    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_begin);

    return duration.count() / 1000.0f;
}


bool time_watcher_t::timed_out(duration_time_t time_out) const
{
    return (time_out >= 0.0) and (duration() >= time_out);
}


const int BUFFER_SIZE_FOR_FMT = 256 * 256;

struct { int year, month, day, hour, minuite, second; } TIME_BEGIN;


void initialize()
{
    now(&TIME_BEGIN.year, &TIME_BEGIN.month, &TIME_BEGIN.day,
        &TIME_BEGIN.hour, &TIME_BEGIN.minuite, &TIME_BEGIN.second);
}


inline void print_console(const std::string &str)
{
    std::lock_guard<std::mutex> lock(g_mutex_for_print);
    std::cerr << time_stamp() << str << std::endl;
}


inline void print_error(const std::string &str)
{
    std::lock_guard<std::mutex> lock(g_mutex_for_print);
    std::cerr
#ifdef _WIN32
        << " * ERROR * "
#else
        << "\33[0;41m * ERROR * \33[0m"
#endif
        << str << std::endl;
}


inline void print_warning(const std::string &str)
{
    std::lock_guard<std::mutex> lock(g_mutex_for_print);
    std::cerr
#ifdef _WIN32
        << " * WARNING * "
#else
        << "\33[0;41m * WARNING * \33[0m"
#endif
        << str << std::endl;
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


inline void now(int *year, int *month, int *day, int *hour, int *min, int *sec)
{
#ifdef _WIN32
    time_t t;
    struct tm ltm;
    time(&t);
    localtime_s(&ltm, &t);

    *year = 1900 + ltm.tm_year;
    *month = 1 + ltm.tm_mon;
    *day = ltm.tm_mday;
    *hour = ltm.tm_hour;
    *min = ltm.tm_min;
    *sec = ltm.tm_sec;
#else
    time_t t;
    tm *p_ltm;
    time(&t);
    p_ltm = localtime(&t);

    *year = 1900 + p_ltm->tm_year;
    *month = 1 + p_ltm->tm_mon;
    *day = p_ltm->tm_mday;
    *hour = p_ltm->tm_hour;
    *min = p_ltm->tm_min;
    *sec = p_ltm->tm_sec;
#endif
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
