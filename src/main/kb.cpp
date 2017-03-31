#include "./kb.h"

namespace dav
{

namespace kb
{

void feature_to_rules_cdb_t::prepare_compile()
{
	cdb_data_t::prepare_compile();
	m_feat2rids.clear();
}


void feature_to_rules_cdb_t::finalize()
{
	char key[256];
	binary_writer_t wr_key(key, 256);

	for (const auto &p : m_feat2rids)
	{
		const conjunction_t::feature_t &feat = p.first.first;
		is_backward_t backward = p.first.second;
		wr_key.reset();
		wr_key.write<conjunction_t::feature_t>(feat);
		wr_key.write<char>(backward ? 1 : 0);

		std::vector<char> val(sizeof(rule_id_t) * p.second.size(), '\0');
		binary_writer_t wr_val(&val[0], val.size());
		for (const auto &rid : p.second)
			wr_val.write<rule_id_t>(rid);

		put(key, wr_key.size(), &val[0], wr_val.size());
	}

	cdb_data_t::finalize();
}


std::list<rule_id_t> feature_to_rules_cdb_t::gets(
	const conjunction_t::feature_t &feat, is_backward_t backward) const
{
	assert(is_readable());

	std::list<rule_id_t> out;
	char key[512];
	binary_writer_t key_writer(key, 512);

	key_writer.write<conjunction_t::feature_t>(feat);
	key_writer.write<char>(backward ? 1 : 0);

	size_t value_size;
	const char *value = (const char*)get(key, key_writer.size(), &value_size);

	if (value != nullptr)
	{
		binary_reader_t val_reader(value, value_size);
		size_t num(0);

		val_reader.read<size_t>(&num);
		for (size_t i = 0; i < num; ++i)
		{
			rule_id_t rid;
			val_reader.read<rule_id_t>(&rid);
			out.push_back(rid);
		}
	}

	return out;
}


void feature_to_rules_cdb_t::insert(const conjunction_t &conj, is_backward_t backward, rule_id_t rid)
{
	m_feat2rids[std::make_pair(conj.feature(), backward)].insert(rid);
}


}

}