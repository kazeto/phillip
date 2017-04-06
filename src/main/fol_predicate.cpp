#include "./fol.h"

namespace dav
{

void parse(const string_t &str, string_t *pred, arity_t *arity)
{
    auto i = str.rfind('/');
    assert(i >= 0);

    *pred = str.substr(0, i);
    *arity = std::stoi(str.substr(i + 1));
}


predicate_t::predicate_t(const string_t &s, arity_t a)
    : m_pred(s), m_arity(a)
{
    m_pid = predicate_library_t::instance()->pred2id(string());
}


predicate_t::predicate_t(const string_t &s)
{
    parse(s, &m_pred, &m_arity);
    m_pid = predicate_library_t::instance()->pred2id(string());
}


predicate_t::predicate_t(predicate_id_t pid)
    : m_pid(pid)
{
    auto p = predicate_library_t::instance()->id2pred(pid);
	m_pred = p.predicate();
	m_arity = p.arity();
}


predicate_t::predicate_t(std::ifstream *fi)
{
    char line[256];
    small_size_t num_char;
    fi->read((char*)&num_char, sizeof(small_size_t));
    fi->read(line, sizeof(char)* num_char);
    line[num_char] = '\0';

    parse(line, &m_pred, &m_arity);

    // m_pid WILL BE GIVEN IN predicate_library_t::read.
}


void predicate_t::write(std::ofstream *fo) const
{
    string_t s = string();
    small_size_t len = static_cast<small_size_t>(s.length());
    fo->write((char*)&len, sizeof(small_size_t));
    fo->write(s.c_str(), sizeof(char)* len);
}


bool predicate_t::operator>(const predicate_t &x) const
{
    if (m_pid != x.m_pid) return m_pid > x.m_pid;
    if (m_arity != x.m_arity) return m_arity > x.m_arity;
    return (m_pred > x.m_pred);
}


bool predicate_t::operator<(const predicate_t &x) const
{
    if (m_pid != x.m_pid) return m_pid < x.m_pid;
    if (m_arity != x.m_arity) return m_arity < x.m_arity;
    return (m_pred < x.m_pred);
}


bool predicate_t::operator==(const predicate_t &x) const
{
    if (m_pid != x.m_pid) return false;
    if (m_arity != x.m_arity) return false;
    return (m_pred == x.m_pred);
}


bool predicate_t::operator!=(const predicate_t &x) const
{
    if (m_pid != x.m_pid) return true;
    if (m_arity != x.m_arity) return true;
    return (m_pred != x.m_pred);
}


void predicate_t::assign()
{
	m_pid = plib()->add(*this);
}


string_t predicate_t::string() const
{
    char line[512];
    _sprintf(line, "%s/%d", m_pred.c_str(), m_arity);
    return line;
}


bool predicate_t::good() const
{
    return not m_pred.empty() and m_arity > 0;
}


}