#include "./kb.h"

namespace dav
{

namespace kb
{

std::mutex rule_library_t::ms_mutex;



rule_library_t::rule_library_t(const filepath_t &filename)
	: m_filename(filename),
	m_num_rules(0), m_num_unnamed_rules(0), m_writing_pos(0)
{}


rule_library_t::~rule_library_t()
{
	finalize();
}


void rule_library_t::prepare_compile()
{
	if (is_readable())
		finalize();

	if (not is_writable())
	{
		std::lock_guard<std::mutex> lock(ms_mutex);

		auto flag = (std::ios::binary | std::ios::out);
		m_fo_idx.reset(new std::ofstream(filepath_idx().c_str(), flag));
		m_fo_dat.reset(new std::ofstream(filepath_dat().c_str(), flag));
		m_num_rules = 0;
		m_num_unnamed_rules = 0;
		m_writing_pos = 0;
	}
}


void rule_library_t::prepare_query()
{
	if (is_writable())
		finalize();

	if (not is_readable())
	{
		std::lock_guard<std::mutex> lock(ms_mutex);

		auto flag = (std::ios::binary | std::ios::in);
		m_fi_idx.reset(new std::ifstream(filepath_idx().c_str(), flag));
		m_fi_dat.reset(new std::ifstream(filepath_dat().c_str(), flag));

		m_fi_idx->seekg(-static_cast<int>(sizeof(int)), std::ios_base::end);
		m_fi_idx->read((char*)&m_num_rules, sizeof(size_t));
		m_fi_idx->clear();
	}
}


void rule_library_t::finalize()
{
	if (is_writable())
	{
		m_fo_idx->write((char*)&m_num_rules, sizeof(size_t));
		m_fo_idx.reset();
		m_fo_dat.reset();
	}

	m_fi_idx.reset();
	m_fi_dat.reset();
}


rule_id_t rule_library_t::add(rule_t &r)
{
	if (r.name().empty())
		r.name() = get_name_of_unnamed_axiom();

	rule_id_t id = size() + 1;
	r.rid() = id;

	// WRITE AXIOM TO KNOWLEDGE-BASE.
	const int SIZE(512 * 512);
	char buffer[SIZE];
	binary_writer_t wr(buffer, SIZE);

	/* AXIOM => BINARY-DATA */
	wr.write<rule_t>(r);

	/* INSERT A RULE TO CDB.IDX */
	size_t rsize(wr.size());
	m_fo_idx->write((char*)(&m_writing_pos), sizeof(pos_t));
	m_fo_idx->write((char*)(&rsize), sizeof(size_t));

	m_fo_dat->write(buffer, wr.size());

	++m_num_rules;
	m_writing_pos += wr.size();

	return id;
}


rule_t rule_library_t::get(rule_id_t rid) const
{
	assert(rid <= size());

	std::lock_guard<std::mutex> lock(ms_mutex);

	assert(is_readable());

	pos_t pos;
	size_t rsize;
	const int SIZE(512 * 512);
	char buffer[SIZE];

	// GET RULE'S POSITION AND SIZE.
	int offset = (rid - 1) * (sizeof(pos_t) + sizeof(size_t));
	m_fi_idx->seekg(offset);
	m_fi_idx->read((char*)&pos, sizeof(pos_t));
	m_fi_idx->read((char*)&rsize, sizeof(size_t));

	// GET THE RULE
	m_fi_dat->seekg(pos);
	m_fi_dat->read(buffer, rsize);

	rule_t out;
	binary_reader_t(buffer, SIZE).read<rule_t>(&out);
	out.rid() = rid;

	return out;
}


string_t rule_library_t::get_name_of_unnamed_axiom()
{
	char buf[128];
	_sprintf(buf, "_%#.8lx", m_num_unnamed_rules++);
	return string_t(buf);
}


}

}