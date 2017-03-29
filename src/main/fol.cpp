#include "./fol.h"

namespace dav
{

conjunction_t::conjunction_t(binary_reader_t &r)
{
	small_size_t len;
	r.read<small_size_t>(&len);

	assign(len, literal_t());
	for (small_size_t i = 0; i < len; ++i)
		at(i) = literal_t(r);

	r.read<std::string>(&m_param);
}


conjunction_t::feature_t conjunction_t::feature() const
{
	feature_t out;

	for (const auto &a : (*this))
		out.pids.push_back(a.predicate().pid());

	return out;
}


conjunction_t::feature_t::feature_t(binary_reader_t &r)
{
	small_size_t len;
	r.read<small_size_t>(&len);

	pids.assign(len, 0);
	for (small_size_t i = 0; i < len; ++i)
		r.read<predicate_id_t>(&pids[i]);
}


rule_t::rule_t(binary_reader_t &r)
{
	r.read<std::string>(&m_name);
	m_lhs = conjunction_t(r);
	m_rhs = conjunction_t(r);
}


}