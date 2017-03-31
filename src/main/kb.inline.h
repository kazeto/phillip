#pragma once


namespace dav
{

namespace kb
{

template <typename T> void rules_cdb_t<T>::prepare_compile()
{
	cdb_data_t::prepare_compile();
	m_rids.clear();
}


template <typename T> void rules_cdb_t<T>::finalize()
{
	char key[512];
	binary_writer_t key_writer(key);

	// WRITE TO CDB FILE
	for (auto p : m_rids)
	{
		key_writer.reset();
		key_writer.write<T>(p.first);

		size_t size_value = sizeof(size_t);
		for (const auto &f : p.second)
			size_value += f.bytesize();

		char *value = new char[size_value];
		binary_writer_t value_writer(value);

		value_writer.write<size_t>(p.second.size());
		for (const auto &f : p.second)
			value_writer.write(f);

		assert(size_value == value_writer.size());
		put(key, key_writer.size(), value, value_writer.size());

		delete[] value;
	}

	m_rids.clear();
	cdb_data_t::finalize();
}


template <typename T> std::list<rule_id_t> rules_cdb_t<T>::gets(const T &key) const
{
	assert(is_readable());

	std::list<rule_id_t> out;
	char key[512];
	binary_writer_t key_writer(key);
	key_writer.write<T>(key);

	size_t value_size;
	const char *value = (const char*)get(key, key_writer.size(), &value_size);

	if (value != nullptr)
	{
		binary_reader_t value_reader(value, value_size);
		size_t num(0);
		value_reader.read<size_t>(&num);

		for (size_t i = 0; i < num; ++i)
		{
			rule_id_t rid;
			value_reader.read<rule_id_t>(&rid);
			out.push_back(rid);
		}
	}

	return out;
}


template <typename T> void rules_cdb_t<T>::insert(const T &key, rule_id_t value)
{
	assert(is_writable());
	m_rids[k].insert(v);
}


}

}
