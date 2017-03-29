#include "./fol.h"

namespace dav
{

literal_t literal_t::equal(const term_t &t1, const term_t &t2, bool naf)
{
    return literal_t(EQ_PREDICATE_ID, std::vector<term_t>{t1, t2}, true, naf);
}


literal_t literal_t::not_equal(const term_t &t1, const term_t &t2, bool naf)
{
    return literal_t(EQ_PREDICATE_ID, std::vector<term_t>{t1, t2}, false, naf);
}


literal_t::literal_t(
    predicate_id_t pid, const std::vector<term_t> &terms, bool neg, bool naf)
    : m_predicate(pid), m_terms(terms), m_neg(neg), m_naf(naf)
{
    regularize();
}


literal_t::literal_t(
    const string_t &pred, const std::vector<term_t> &terms, bool neg, bool naf)
    : m_predicate(pred), m_terms(terms), m_neg(neg), m_naf(naf)
{
    regularize();
}


literal_t::literal_t(
    const string_t &pred,
    const std::initializer_list<std::string> &terms, bool neg, bool naf)
    : m_predicate(pred), m_neg(neg), m_naf(naf)
{
    for (auto t : terms)
        m_terms.push_back(term_t(t));
    regularize();
}


bool literal_t::operator > (const literal_t &x) const
{
    if (m_neg != x.m_neg) return not m_neg;
    if (m_naf != x.m_naf) return not m_naf;
    if (m_predicate != x.m_predicate) return (m_predicate > x.m_predicate);

    for (size_t i = 0; i < m_terms.size(); i++)
    {
        if (m_terms[i] != x.m_terms[i])
            return m_terms[i] > x.m_terms[i];
    }
    return false;
}


bool literal_t::operator < (const literal_t &x) const
{
    if (m_neg != x.m_neg) return m_neg;
    if (m_naf != x.m_naf) return m_naf;
    if (m_predicate != x.m_predicate) return (m_predicate < x.m_predicate);

    for (size_t i = 0; i < m_terms.size(); i++)
    {
        if (m_terms[i] != x.m_terms[i])
            return m_terms[i] < x.m_terms[i];
    }
    return false;
}


bool literal_t::operator == (const literal_t &x) const
{
    if (m_neg != x.m_neg) return false;
    if (m_naf != x.m_naf) return false;
    if (m_predicate != x.m_predicate) return false;

    for (size_t i = 0; i < m_terms.size(); i++)
    {
        if (m_terms[i] != x.m_terms[i])
            return false;
    }
    return true;
}


bool literal_t::operator != (const literal_t &x) const
{
    if (m_neg != x.m_neg) return true;
    if (m_naf != x.m_naf) return true;
    if (m_predicate != x.m_predicate) return true;

    for (size_t i = 0; i < m_terms.size(); i++)
    {
        if (m_terms[i] != x.m_terms[i])
            return true;
    }
    return false;
}


/** Get string-expression of the literal. */
string_t literal_t::string() const
{
    static const int color[] = { 31, 32, 33, 34, 35, 36, 37, 38, 39, 40 };
    std::string out;

    if (m_naf) out += "not ";
    if (m_neg) out += "!";

    out += m_predicate.predicate() + '(';

    for (auto it = terms().begin(); it != terms().end(); it++)
    {
        out += it->string();
        if (std::next(it) != terms().end())
            out += ", ";
    }

    out += ")";

    return out;
}


// This method is called only in compiling KB!
size_t literal_t::write_binary(char *bin) const
{
    size_t n(0);

    assert(m_predicate.pid() != INVALID_PREDICATE_ID);
    n += util::to_binary<predicate_id_t>(m_predicate.pid(), bin);

    /* terms */
    n += util::num_to_binary(m_terms.size(), bin + n);
    for (int i = 0; i < m_terms.size(); ++i)
        n += util::string_to_binary(m_terms.at(i).string(), bin + n);

    char flag(0b0000);
    if (m_neg) flag |= 0b0001;
    if (m_naf) flag |= 0b0010;
    n += util::to_binary<char>(flag, bin + n);

    return n;
}


// This method is called only in looking up KB!
size_t literal_t::read_binary(const char *bin)
{
    size_t n(0);
    std::string s_buf;
    int i_buf;

    predicate_id_t pid;
    n += util::binary_to<predicate_id_t>(bin, &pid);
    assert(pid != INVALID_PREDICATE_ID);
    m_predicate = predicate_t(pid);

    n += util::binary_to_num(bin + n, &i_buf);
    m_terms.assign(i_buf, term_t());
    assert(m_predicate.arity() == i_buf);

    for (int i = 0; i<i_buf; ++i)
    {
        n += util::binary_to_string(bin + n, &s_buf);
        m_terms[i] = term_t(s_buf);
    }

    char flag;
    n += util::binary_to<char>(bin + n, &flag);
    m_neg = (bool)(flag & 0b0001);
    m_naf = (bool)(flag & 0b0010);

    return n;
}


bool literal_t::good() const
{
    return m_predicate.good() and (m_predicate.arity() == m_terms.size());
}


inline void literal_t::regularize()
{
    // IF THE PREDICATE IS SYMMETRIC, SORT TERMS.
}



std::ostream& operator<<(std::ostream& os, const literal_t& lit)
{
    return os << lit.string();
}



}