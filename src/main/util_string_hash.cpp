#include "./util.h"

namespace dav
{

std::mutex string_hash_t::ms_mutex_hash;
std::mutex string_hash_t::ms_mutex_unknown;
hash_map<std::string, unsigned> string_hash_t::ms_hashier;
std::deque<string_t> string_hash_t::ms_strs;
unsigned string_hash_t::ms_issued_variable_count = 0;


string_hash_t string_hash_t::get_unknown_hash()
{
    std::lock_guard<std::mutex> lock(ms_mutex_unknown);

    char buffer[128];
    _sprintf(buffer, "_u%d", ++ms_issued_variable_count);
    return string_hash_t(std::string(buffer));
}


void string_hash_t::reset_unknown_hash_count()
{
    std::lock_guard<std::mutex> lock(ms_mutex_unknown);
    ms_issued_variable_count = 0;
}


unsigned string_hash_t::get_hash(std::string str)
{
    std::lock_guard<std::mutex> lock(ms_mutex_hash);
    bool has_shortened = false;

    if (str.length() > 250)
    {
        str = str.substr(0, 250);
        has_shortened = true;
    }

    auto it = ms_hashier.find(str);
    if (it != ms_hashier.end())
        return it->second;
    else
    {
        if (has_shortened)
            console()->warn("The string has been shortened: " + str);

        ms_strs.push_back(str);
        unsigned idx(ms_strs.size() - 1);
        ms_hashier[str] = idx;
        return idx;
    }
}


string_hash_t::string_hash_t(const string_hash_t& h)
    : m_hash(h.m_hash)
{
    set_flags(string());
#ifdef _DEBUG
    m_string = h.string();
#endif
}


string_hash_t::string_hash_t(const std::string &s)
    : m_hash(get_hash(s))
{
    set_flags(s);
#ifdef _DEBUG
    m_string = s;
#endif
}


bool string_hash_t::is_unifiable_with(const string_hash_t &x) const
{
    return this->is_constant() ?
        (not x.is_constant() or x == (*this)) : true;
}


const std::string& string_hash_t::string() const
{
    std::lock_guard<std::mutex> lock(ms_mutex_hash);
    return ms_strs.at(m_hash);
}


string_hash_t::operator const std::string& () const
{
    std::lock_guard<std::mutex> lock(ms_mutex_hash);
    return ms_strs.at(m_hash);
}


string_hash_t& string_hash_t::operator = (const std::string &s)
{
    m_hash = get_hash(s);
    set_flags(s);

#ifdef _DEBUG
    m_string = s;
#endif

    return *this;
}


string_hash_t& string_hash_t::operator = (const string_hash_t &h)
{
    m_hash = h.m_hash;
    set_flags(string());

#ifdef _DEBUG
    m_string = h.string();
#endif

    return *this;
}


void string_hash_t::set_flags(const std::string &str)
{
    assert(not str.empty());

    if (not str.empty())
    {
        m_is_constant = std::isupper(str.at(0));
        m_is_unknown = (str.size() < 2) ? false :
            (str.at(0) == '_' and str.at(1) == 'u');
    }
    else
    {
        m_is_constant = false;
        m_is_unknown = false;
    }
}


}