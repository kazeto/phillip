#include "./kb.h"

namespace dav
{

namespace kb
{


conjunction_library_t::conjunction_library_t(const filepath_t &path)
	: cdb_data_t(path)
{}


void conjunction_library_t::prepare_compile()
{
	cdb_data_t::prepare_compile();
	m_features.clear();
}


void conjunction_library_t::prepare_query()
{
	cdb_data_t::prepare_query();
}


void conjunction_library_t::finalize()
{
	// WRITE TO CDB FILE
	for (auto p : m_features)
	{
		size_t size_value = sizeof(size_t);
		for (const auto &f : p.second)
			size_value += f.bytesize();

		char *value = new char[size_value];
		binary_writer_t writer(value);

		writer.write<size_t>(p.second.size());
		for (const auto &f : p.second)
			writer.write(f);

		put((char*)(&p.first), sizeof(predicate_id_t), value, size_value);

		delete[] value;
	}
	m_features.clear();

	cdb_data_t::finalize();
}


void conjunction_library_t::insert(const rule_t &r)
{
	assert(is_writable());

	auto f_lhs = r.lhs().feature();
	f_lhs.is_rhs = false;

	for (const auto &a : r.lhs())
		m_features[a.predicate().pid()].insert(f_lhs);

	auto f_rhs = r.lhs().feature();
	f_rhs.is_rhs = true;

	for (const auto &a : r.rhs())
		m_features[a.predicate().pid()].insert(f_rhs);
}


std::list<conjunction_t::feature_t> conjunction_library_t::get(predicate_id_t pid) const
{
	assert(is_readable());

	std::list<conjunction_t::feature_t> out;

	size_t value_size;
	const char *value =
		(const char*)cdb_data_t::get(&pid, sizeof(predicate_id_t), &value_size);

	if (value != nullptr)
	{
		binary_reader_t reader(value);
		size_t num;

		reader.read<size_t>(&num);
		out.assign(num, conjunction_t::feature_t());
		for (auto &p : out)
			reader.read<conjunction_t::feature_t>(&p);
	}

	return out;
}


} // end of kb

} // end of dav