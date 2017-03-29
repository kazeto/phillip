#include "./fol.h"

namespace dav
{

std::unique_ptr<conjunction_library_t> conjunction_library_t::ms_instance;


void conjunction_library_t::initialize(const filepath_t &path)
{
	ms_instance.reset(new conjunction_library_t(path));
}


conjunction_library_t* conjunction_library_t::instance()
{
	assert(not ms_instance);
	return ms_instance.get();
}


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


void conjunction_library_t::insert(const conjunction_t &conj)
{
	assert(is_writable());

	auto feat = conj.feature();

	for (const auto &a : conj)
		m_features[a.predicate().pid()].insert(feat);
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


}