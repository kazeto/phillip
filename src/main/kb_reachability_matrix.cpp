#include "./kb.h"

namespace dav
{

namespace kb
{


std::unique_ptr<reachability_matrix_t> reachability_matrix_t::ms_instance;
std::mutex reachability_matrix_t::ms_mutex;


void reachability_matrix_t::initialize(const filepath_t &path)
{
	ms_instance.reset(new reachability_matrix_t(path));
}


reachability_matrix_t* reachability_matrix_t::instance()
{
	return ms_instance.get();
}


reachability_matrix_t::reachability_matrix_t(const filepath_t &path)
	: m_path(path)
{}


void reachability_matrix_t::prepare_compile()
{
	if (is_readable())
		finalize();

	if (not is_writable())
	{
		std::lock_guard<std::mutex> lock(ms_mutex);
		pos_t pos;

		m_fout.reset(new std::ofstream(m_path.c_str(), std::ios::binary | std::ios::out));
		m_fout->write((const char*)&pos, sizeof(pos_t));
	}
}


void reachability_matrix_t::prepare_query()
{
	if (is_writable())
		finalize();

	if (not is_readable())
	{
		std::lock_guard<std::mutex> lock(ms_mutex);
		pos_t pos;
		size_t num, idx;

		m_fin.reset(new std::ifstream(m_path.c_str(), std::ios::binary | std::ios::in));

		m_fin->read((char*)&pos, sizeof(pos_t));
		m_fin->seekg(pos, std::ios::beg);

		m_fin->read((char*)&num, sizeof(size_t));
		for (size_t i = 0; i < num; ++i)
		{
			m_fin->read((char*)&idx, sizeof(idx));
			m_fin->read((char*)&pos, sizeof(pos_t));
			m_map_idx_to_pos[idx] = pos;
		}
	}
}


void reachability_matrix_t::finalize()
{
	if (m_fout)
	{
		std::lock_guard<std::mutex> lock(ms_mutex);
		pos_t pos = m_fout->tellp();
		size_t num = m_map_idx_to_pos.size();

		m_fout->write((const char*)&num, sizeof(size_t));

		for (auto it = m_map_idx_to_pos.begin(); it != m_map_idx_to_pos.end(); ++it)
		{
			m_fout->write((const char*)&it->first, sizeof(size_t));
			m_fout->write((const char*)&it->second, sizeof(pos_t));
		}

		m_fout->seekp(0, std::ios::beg);
		m_fout->write((const char*)&pos, sizeof(pos_t));

		m_fout.reset();
	}

	if (m_fin)
		m_fin.reset();

	m_map_idx_to_pos.clear();
}


void reachability_matrix_t::
put(size_t idx1, const hash_map<size_t, float> &dist)
{
	std::lock_guard<std::mutex> lock(ms_mutex);
	size_t num(0);
	m_map_idx_to_pos[idx1] = m_fout->tellp();

	for (auto it = dist.begin(); it != dist.end(); ++it)
		if (idx1 <= it->first)
			++num;

	m_fout->write((const char*)&num, sizeof(size_t));
	for (auto it = dist.begin(); it != dist.end(); ++it)
	{
		if (idx1 <= it->first)
		{
			m_fout->write((const char*)&it->first, sizeof(size_t));
			m_fout->write((const char*)&it->second, sizeof(float));
		}
	}
}


float reachability_matrix_t::get(size_t idx1, size_t idx2) const
{
	if (idx1 > idx2) std::swap(idx1, idx2);

	std::lock_guard<std::mutex> lock(ms_mutex);
	size_t num, idx;
	float dist;
	auto find = m_map_idx_to_pos.find(idx1);

	if (find == m_map_idx_to_pos.end()) return -1.0f;

	m_fin->seekg(find->second, std::ios::beg);
	m_fin->read((char*)&num, sizeof(size_t));

	for (size_t i = 0; i < num; ++i)
	{
		m_fin->read((char*)&idx, sizeof(size_t));
		m_fin->read((char*)&dist, sizeof(float));
		if (idx == idx2) return dist;
	}

	return -1.0f;
}


hash_set<float> reachability_matrix_t::get(size_t idx) const
{
	std::lock_guard<std::mutex> lock(ms_mutex);
	size_t num;
	float dist;
	hash_set<float> out;
	auto find = m_map_idx_to_pos.find(idx);

	if (find == m_map_idx_to_pos.end()) return out;

	m_fin->seekg(find->second, std::ios::beg);
	m_fin->read((char*)&num, sizeof(size_t));

	for (size_t i = 0; i < num; ++i)
	{
		m_fin->read((char*)&idx, sizeof(size_t));
		m_fin->read((char*)&dist, sizeof(float));
		out.insert(dist);
	}

	return out;
}


}

}