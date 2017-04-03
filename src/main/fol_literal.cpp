#include "./fol.h"

namespace dav
{

atom_t atom_t::equal(const term_t &t1, const term_t &t2, bool naf)
{
    return atom_t(EQ_PREDICATE_ID, std::vector<term_t>{t1, t2}, true, naf);
}


atom_t atom_t::not_equal(const term_t &t1, const term_t &t2, bool naf)
{
    return atom_t(EQ_PREDICATE_ID, std::vector<term_t>{t1, t2}, false, naf);
}


atom_t::atom_t(
    predicate_id_t pid, const std::vector<term_t> &terms, bool neg, bool naf)
    : m_predicate(pid), m_terms(terms), m_neg(neg), m_naf(naf)
{
    regularize();
}


atom_t::atom_t(
    const string_t &pred, const std::vector<term_t> &terms, bool neg, bool naf)
    : m_predicate(pred, terms.size()), m_terms(terms), m_neg(neg), m_naf(naf)
{
    regularize();
}


atom_t::atom_t(
    const string_t &pred,
    const std::initializer_list<std::string> &terms, bool neg, bool naf)
    : m_predicate(pred, terms.size()), m_neg(neg), m_naf(naf)
{
    for (auto t : terms)
        m_terms.push_back(term_t(t));
    regularize();
}


atom_t::atom_t(binary_reader_t &r)
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


bool atom_t::operator > (const atom_t &x) const
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


bool atom_t::operator < (const atom_t &x) const
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


bool atom_t::operator == (const atom_t &x) const
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


bool atom_t::operator != (const atom_t &x) const
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
string_t atom_t::string() const
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


bool atom_t::good() const
{
    return m_predicate.good() and (m_predicate.arity() == m_terms.size());
}


inline void atom_t::regularize()
{
	auto prp = predicate_library_t::instance()->find_property(predicate().pid());

	if ((bool)prp)
	{
		// IF THE PREDICATE IS SYMMETRIC, SORT TERMS.
		if (prp->is_symmetric())
		{
			auto ar = m_terms.size();
			if (ar > 1)
				if (term(ar - 2) > term(ar - 1))
					std::swap(m_terms[ar - 2], m_terms[ar - 1]);
		}
	}
}



std::ostream& operator<<(std::ostream& os, const atom_t& lit)
{
    return os << lit.string();
}


template <> void binary_writer_t::write<atom_t>(const atom_t &x)
{
	assert(x.predicate().pid() != INVALID_PREDICATE_ID);
	write<predicate_id_t>(x.predicate().pid());

	// WRITE ARGUMENTS
	for (int i = 0; i < x.terms().size(); ++i)
		write<std::string>(x.term(i).string());

	// WRITE NEGATION
	char flag(0b0000);
	if (x.neg()) flag |= 0b0001;
	if (x.naf()) flag |= 0b0010;
	write<char>(flag);

	// WRITE PARAMETER
	write<std::string>(x.param());
}


}