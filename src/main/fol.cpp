#include <algorithm>

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
	{
		auto pid = a.predicate().pid();
		if (pid != INVALID_PREDICATE_ID and pid != EQ_PREDICATE_ID)
			out.pids.push_back(pid);
	}

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


bool conjunction_t::feature_t::operator<(const feature_t &x) const
{
	return pids < x.pids;
}


bool conjunction_t::feature_t::operator>(const feature_t &x) const
{
	return pids > x.pids;
}


bool conjunction_t::feature_t::operator==(const feature_t &x) const
{
	return pids == x.pids;
}


bool conjunction_t::feature_t::operator!=(const feature_t &x) const
{
	return pids != x.pids;
}


char* conjunction_t::feature_t::binary() const
{
	char *out = new char[bytesize()];
	binary_writer_t wr(out, bytesize());
	wr.write(*this);
	return out;
}


size_t conjunction_t::feature_t::bytesize() const
{
	return sizeof(predicate_id_t) * pids.size() + sizeof(char);
}


bool conjunction_t::feature_t::entail(const feature_t&)
{

}


rule_t::rule_t(binary_reader_t &r)
{
	r.read<std::string>(&m_name);
	m_lhs = conjunction_t(r);
	m_rhs = conjunction_t(r);
}


}