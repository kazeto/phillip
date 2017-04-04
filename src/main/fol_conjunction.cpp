#include <algorithm>

#include "./fol.h"


namespace dav
{

conjunction_t::conjunction_t(binary_reader_t &r)
{
	small_size_t len;
	r.read<small_size_t>(&len);

	assign(len, atom_t());
	for (small_size_t i = 0; i < len; ++i)
		at(i) = atom_t(r);

	r.read<std::string>(&m_param);
}


string_t conjunction_t::string() const
{
	std::list<string_t> strs;
	for (const auto &a : (*this))
		strs.push_back(a.string());

	return "{" + join(strs.begin(), strs.end(), " ^ ") + "}";
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


template <> void binary_writer_t::write<conjunction_t>(const conjunction_t &x)
{
	write<small_size_t>(static_cast<small_size_t>(x.size()));
	for (const auto &a : x)
		write<atom_t>(a);
	write<std::string>(x.param());
}


template <> void binary_reader_t::read<conjunction_t>(conjunction_t *p)
{
	*p = conjunction_t(*this);
}


template <> void binary_writer_t::
write<conjunction_t::feature_t>(const conjunction_t::feature_t &x)
{
	write<small_size_t>(static_cast<small_size_t>(x.pids.size()));
	for (const auto &pid : x.pids)
		write<predicate_id_t>(pid);
}


template <> void binary_reader_t::read<conjunction_t::feature_t>(conjunction_t::feature_t *p)
{
	*p = conjunction_t::feature_t(*this);
}

}