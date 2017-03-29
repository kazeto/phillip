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


literal_t::literal_t(binary_reader_t &r)
{
	std::string s_buf;
	predicate_id_t pid;

	r.read<predicate_id_t>(&pid);
	assert(pid != INVALID_PREDICATE_ID);
	m_predicate = predicate_t(pid);

	// READ ARGUMENTS
	m_terms.assign(m_predicate.arity(), term_t());
	for (int i = 0; i<m_predicate.arity(); ++i)
	{
		r.read<std::string>(&s_buf);
		m_terms[i] = term_t(s_buf);
	}

	// READ NEGATION
	char flag;
	r.read<char>(&flag);
	m_neg = (bool)(flag & 0b0001);
	m_naf = (bool)(flag & 0b0010);

	// READ PARAMETER
	r.read<std::string>(&m_param);
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