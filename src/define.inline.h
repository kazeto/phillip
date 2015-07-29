/* -*- coding: utf-8 -*- */

#pragma once

#include <sstream>
#include <cassert>


namespace phil
{


inline string_hash_t::string_hash_t()
: m_hash(0)
{}


inline string_hash_t::string_hash_t(const string_hash_t& h)
: m_hash(h.m_hash)
{
    set_flags(string());
#ifdef _DEBUG
    m_string = h.string();
#endif
}


inline string_hash_t::string_hash_t( const std::string &s )
    : m_hash(get_hash(s))
{
    set_flags(s);
#ifdef _DEBUG
    m_string = s;
#endif
}


inline string_hash_t string_hash_t::get_unknown_hash()
{
    std::lock_guard<std::mutex> lock(ms_mutex_unknown);

    char buffer[128];
    _sprintf(buffer, "_u%d", ++ms_issued_variable_count);
    return string_hash_t(std::string(buffer));
}


inline void string_hash_t::reset_unknown_hash_count()
{
    std::lock_guard<std::mutex> lock(ms_mutex_unknown);
    ms_issued_variable_count = 0;
}


inline unsigned string_hash_t::get_hash(std::string str)
{
    std::lock_guard<std::mutex> lock(ms_mutex_hash);

    hash_map<std::string, unsigned>::iterator it = ms_hashier.find(str);
    if (it != ms_hashier.end())
        return it->second;
    else
    {
        ms_strs.push_back(str);
        unsigned idx(ms_strs.size() - 1);
        ms_hashier[str] = idx;
        return idx;
    }
}


inline const std::string& string_hash_t::string() const
{
    std::lock_guard<std::mutex> lock(ms_mutex_hash);
    return ms_strs.at(m_hash);
}


inline string_hash_t::operator const std::string& () const
{
    std::lock_guard<std::mutex> lock(ms_mutex_hash);
    return ms_strs.at(m_hash);
}


inline string_hash_t& string_hash_t::operator = (const std::string &s)
{
    m_hash = get_hash(s);
    set_flags(s);

#ifdef _DEBUG
    m_string = s;
#endif

    return *this;
}


inline string_hash_t& string_hash_t::operator = (const string_hash_t &h)
{
    m_hash = h.m_hash;
    set_flags(string());

#ifdef _DEBUG
    m_string = h.string();
#endif

    return *this;
}


inline bool string_hash_t::operator > (const string_hash_t &x) const
{
    return m_hash > x.m_hash;
}


inline bool string_hash_t::operator < (const string_hash_t &x) const
{
    return m_hash < x.m_hash;
}


inline bool string_hash_t::operator == (const char *s) const
{
    return m_hash == ms_hashier.at(s);
}


inline bool string_hash_t::operator != (const char *s) const
{
    return not(*this == s);
}


inline bool string_hash_t::operator == (const string_hash_t &h) const
{
    return m_hash == h.m_hash;
}


inline bool string_hash_t::operator != (const string_hash_t &h) const
{
    return m_hash != h.m_hash;
}


inline void string_hash_t::set_flags(const std::string &str)
{
    assert(not str.empty());
    if (not str.empty())
    {
        m_is_constant = std::isupper(str.at(0));
        m_is_unknown = (str.size() < 2) ? false :
            (str.at(0) == '_' and str.at(1) == 'u');
#ifdef DISABLE_HARD_TERM
        m_is_hard_term = false;
#else        
        m_is_hard_term = str.empty() ? false : (str.front() == '*');
#endif
    }
    else
    {
        m_is_constant = false;
        m_is_unknown = false;
        m_is_hard_term = false;
    }
}


inline std::string literal_t::get_arity(
    const predicate_t &pred, int term_num, bool is_negated)
{
    return 
        (is_negated ? "!" : "") +
        util::format("%s/%d", pred.c_str(), term_num);
}


inline literal_t::literal_t( const std::string &_pred, bool _truth )
    : predicate(_pred), truth(_truth) {}
    

inline literal_t::literal_t(
    predicate_t _pred, const std::vector<term_t> _terms, bool _truth )
    : predicate(_pred), terms(_terms), truth(_truth)
{
    regularize();
}


inline literal_t::literal_t(
    const std::string &_pred,
    const std::vector<term_t> _terms, bool _truth )
    : predicate(_pred), terms(_terms), truth(_truth)
{
    regularize();
}


inline literal_t::literal_t(
    const std::string &_pred,
    const std::initializer_list<std::string> &_terms, bool _truth)
    : predicate(_pred), truth(_truth)
{
    for (auto t : _terms)
        terms.push_back(term_t(t));
    regularize();
}


inline literal_t::literal_t(
    const std::string &_pred,
    const term_t &term1, const term_t &term2, bool _truth )
    : predicate(_pred), truth(_truth)
{
    terms.push_back(term1);
    terms.push_back(term2);
    regularize();
}


inline literal_t::literal_t(
    const std::string &_pred,
    const std::string &term1, const std::string &term2,
    bool _truth )
    : predicate(_pred), truth(_truth)
{
    terms.push_back( string_hash_t(term1) );
    terms.push_back( string_hash_t(term2) );
    regularize();
}


inline std::string literal_t::to_string( bool f_colored ) const
{
    std::string exp;
    print(&exp, f_colored);
    return exp;
}


inline std::string literal_t::get_arity() const
{
    return get_arity(predicate, terms.size(), not truth);
}


inline void literal_t::regularize()
{
    if (is_equality())
        if (terms.at(0) > terms.at(1))
            std::swap(terms[0], terms[1]);
}


namespace util
{


inline bool timeout_t::do_time_out(duration_time_t duration) const
{
    return not this->empty() and duration >= m_time;
}


inline bool timeout_t::do_time_out(const std::chrono::system_clock::time_point &begin) const
{
    return do_time_out(duration_time(begin));
}



inline void cdb_data_t::put(
    const void *key, size_t ksize, const void *value, size_t vsize)
{
    if (is_writable())
        m_builder->put(key, ksize, value, vsize);
}


inline const void* cdb_data_t::get(
    const void *key, size_t ksize, size_t *vsize) const
{
    return is_readable() ? m_finder->get(key, ksize, vsize) : NULL;
}


inline size_t cdb_data_t::size() const
{
    return is_readable() ? m_finder->size() : 0;
}


inline void xml_element_t::add_attribute(const std::string &key, const std::string &value)
{
    m_attr[key] = value;
}


inline void xml_element_t::remove_attribute(const std::string &key)
{
    m_attr.erase(key);
}


inline void xml_element_t::add_child(const xml_element_t &elem)
{
    m_children.push_back(elem);
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


inline bool do_exist_file(const std::string &path)
{
    bool out(true);
    std::ifstream fin(path);
    if (not fin)
        out = false;
    else
        fin.close();
    return out;
}


inline std::string get_file_name( const std::string &path )
{
#ifdef _WIN32
    int idx = path.rfind("\\");
#else
    int idx = path.rfind("/");
#endif
    return ( idx >= 0 ) ? path.substr(idx+1) : path;
}


inline std::string get_directory_name(const std::string &path)
{
#ifdef _WIN32
    int idx = path.rfind("\\");
#else
    int idx = path.rfind("/");
#endif
    return (idx >= 0) ? path.substr(0, idx) : "";
}


inline size_t get_file_size(const std::string &filename)
{
    struct stat filestatus;
    stat( filename.c_str(), &filestatus );
    return filestatus.st_size;
}

  
inline size_t get_file_size(std::istream &ifs)
{
    size_t file_size =
        static_cast<size_t>(ifs.seekg(0, std::ios::end).tellg());
    ifs.seekg(0, std::ios::beg);
    return file_size;
}


inline bool parse_arity(const arity_t &arity, predicate_t *pred, int *num_term)
{
    int idx = arity.rfind('/');
    if (idx != std::string::npos)
    {
        if (pred != NULL)
            (*pred) = arity.substr(0, idx);
        if (num_term != NULL)
            _sscanf(arity.substr(idx + 1).c_str(), "%d", num_term);
        return true;
    }
    else
        return false;
}


inline size_t string_to_binary(const std::string &str, char *out)
{
    size_t n(0);
    unsigned char size = static_cast<unsigned char>(str.size());

    std::memcpy(out + n, &size, sizeof(unsigned char));
    n += sizeof(unsigned char);

    std::memcpy(out + n, str.c_str(), sizeof(char)* str.size());
    n += sizeof(char)* str.size();

    return n;
}


inline size_t num_to_binary(const int num, char *out)
{
    unsigned char n = static_cast<unsigned char>(num);
    std::memcpy(out, &n, sizeof(unsigned char));
    return sizeof(unsigned char);
}


inline size_t bool_to_binary(const bool _bool, char *out)
{
    char c = _bool ? 1 : 0;
    std::memcpy(out, &c, sizeof(char));
    return sizeof(char);
}


template <class T> inline size_t to_binary(const T &value, char *out)
{
    std::memcpy(out, &value, sizeof(T));
    return sizeof(T);
}


inline size_t binary_to_string(const char *bin, std::string *out)
{
    size_t n(0);
    unsigned char size;
    char str[512];

    std::memcpy(&size, bin, sizeof(unsigned char));
    n += sizeof(unsigned char);

    std::memcpy(str, bin + n, sizeof(char)*size);
    str[size] = '\0';
    *out = std::string(str);
    n += sizeof(char)*size;

    return n;
}


inline size_t binary_to_num(const char *bin, int *out)
{
    unsigned char num;
    std::memcpy(&num, bin, sizeof(unsigned char));
    *out = static_cast<int>(num);
    return sizeof(unsigned char);
}


inline size_t binary_to_bool(const char *bin, bool *out)
{
    char c;
    std::memcpy(&c, bin, sizeof(char));
    *out = (c != 0);
    return sizeof(char);
}


template <class T> inline size_t binary_to(const char *bin, T *out)
{
    size_t size = sizeof(T);
    std::memcpy(out, bin, size);
    return size;
}


template <class It> std::string join(
    const It &s_begin, const It &s_end, const std::string &delimiter)
{
    std::ostringstream ss;
    for (It it = s_begin; it != s_end; ++it)
        ss << (it == s_begin ? "" : delimiter) << (*it);
    return ss.str();
}


template <class Container, class Function> std::string join_f(
    const Container &container, Function func, const std::string &delim)
{
    std::string out;
    for (auto e : container)
        out += (out.empty() ? "" : delim) + func(e);
    return out;
}


template <class Map, class Key, class Value, class Operator>
inline bool find_then(
    const Map &map, const Key &key, const Value &value, const Operator &opr)
{
    Map::const_iterator it = map.find(key);
    if (it == map.end()) return false;
    return opr(it->second, value);
}


template <class It> bool has_intersection(
    It s1_begin, It s1_end, It s2_begin, It s2_end)
{
    for (It i1 = s1_begin; i1 != s1_end; ++i1)
    for (It i2 = s2_begin; i2 != s2_end; ++i2)
    if (*i1 == *i2)
        return true;

    return false;
}


template <class T> hash_set<T> intersection(
    const hash_set<T> &set1, const hash_set<T> &set2)
{
    bool set1_is_smaller = (set1.size() < set2.size());
    const hash_set<T> *smaller = (set1_is_smaller ? &set1 : &set2);
    const hash_set<T> *bigger = (set1_is_smaller ? &set2 : &set1);
    hash_set<T> out;

    for (auto it = smaller->begin(); it != smaller->end(); ++it)
    {
        if (bigger->find(*it) != bigger->end())
            out.insert(*it);
    }

    return out;
}


template <class Container, class Element>
inline bool has_element(const Container &c, const Element &e)
{
    return c.find(e) != c.end();
}


template <class T> std::pair<T, T> make_sorted_pair(const T &x, const T &y)
{
    return (x < y) ? std::make_pair(x, y) : std::make_pair(y, x);
}


template <class Container> void erase(Container &c, size_t i)
{
    auto it = c.begin();
    for (size_t j = 0; j < i; ++j) ++it;
    c.erase(it);
}


} // end of util

} // end of phil

