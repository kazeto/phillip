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
			size_value += f.first.bytesize() + sizeof(char);

		char *value = new char[size_value];
		binary_writer_t writer(value, size_value);

		writer.write<size_t>(p.second.size());
		for (const auto &f : p.second)
		{
			writer.write<conjunction_t::feature_t>(f.first);
			writer.write<char>(f.second ? 1 : 0);
		}

		put((char*)(&p.first), sizeof(predicate_id_t), value, size_value);

		delete[] value;
	}
	m_features.clear();

	cdb_data_t::finalize();
}


void conjunction_library_t::insert(const rule_t &r)
{
	assert(is_writable());

	std::pair<conjunction_t::feature_t, is_backward_t> vl(r.lhs().feature(), false);
	std::pair<conjunction_t::feature_t, is_backward_t> vr(r.rhs().feature(), true);

	for (const auto &a : r.lhs())
		m_features[a.predicate().pid()].insert(vl);

	for (const auto &a : r.rhs())
		m_features[a.predicate().pid()].insert(vr);
}


std::list<conjunction_library_t::elem_t> conjunction_library_t::get(predicate_id_t pid) const
{
	assert(is_readable());

	std::list<elem_t> out;

	size_t value_size;
	const char *value =
		(const char*)cdb_data_t::get(&pid, sizeof(predicate_id_t), &value_size);

	if (value != nullptr)
	{
		binary_reader_t reader(value, value_size);
		size_t num;

		reader.read<size_t>(&num);
		out.assign(num, elem_t());
		for (auto &p : out)
		{
			reader.read<conjunction_t::feature_t>(&p.feature);

			char flag;
			reader.read<char>(&flag);
			p.is_backward = (bool)(flag);
		}
	}

	return out;
}


} // end of kb

} // end of dav