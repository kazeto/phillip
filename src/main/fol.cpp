#include <algorithm>

#include "./fol.h"


namespace dav
{

rule_t::rule_t(binary_reader_t &r)
{
	r.read<std::string>(&m_name);
	m_lhs = conjunction_t(r);
	m_rhs = conjunction_t(r);
}


template <> void binary_writer_t::write<rule_t>(const rule_t &x)
{
	write<std::string>(x.name());
	write<conjunction_t>(x.lhs());
	write<conjunction_t>(x.rhs());
}


}